// paths — builds the light order for each path.
//
// Physical layout (facing the house): controller in the middle, strand A runs LEFT,
// strand B runs RIGHT, light 1 of each next to the controller.
//
// 1. Whole house (built in): starting at the far right end of B, back to the controller,
//    then out along A to the far left end — one sweep across the front, right to left:
//      last B light .. B1, then A1 .. last A light (lengths from ledCount[])
//
// 2. Paths made from zones: a name plus an ordered list of zones, each optionally
//    reversed, each with optional invisible "gap" lights after it. Saved to /paths.bin
//    (header + per slot: PathDef then that path's LightState). Rebuilt whenever a zone
//    changes, since they follow the zones' current lights.

const char*    PATHS_FILE    = "/paths.bin";
const uint32_t PATHS_MAGIC   = 0x48544150;  // "PATH"
const uint16_t PATHS_VERSION = 2;   // v1 (before 0.15.0): settings without the color list

// Builds the Whole house path from the current strand lengths.
void initPaths() {
  int p = 0;
  for (int i = ledCount[1] - 1; i >= 0; i--) housePath[p++] = { 1, (uint16_t)i };  // last B -> B1
  for (int i = 0; i < ledCount[0]; i++)      housePath[p++] = { 0, (uint16_t)i };  // A1 -> last A
  houseLen = p;
}

void initZonePaths() {
  for (int p = 0; p < MAX_PATHS; p++) {
    pathDefs[p].used = false;
    pathDefs[p].name[0] = 0;
    pathDefs[p].stepCount = 0;
    pathLen[p] = 0;
    states[CH_PATH0 + p] = PATH_DEFAULT_STATE;
  }
}

// Rebuilds path p's positions from its zones. Steps whose zone was deleted are skipped.
void rebuildZonePath(int p) {
  uint16_t n = 0;
  if (pathDefs[p].used) {
    for (int s = 0; s < pathDefs[p].stepCount; s++) {
      const PathStep& st = pathDefs[p].steps[s];
      if (st.zone >= MAX_ZONES || !zones[st.zone].used) continue;
      int len = zoneLen[st.zone];
      for (int i = 0; i < len && n < MAX_PATH_LEN; i++) {
        pathPoints[p][n++] = zonePoints[st.zone][st.reverse ? (len - 1 - i) : i];
      }
      for (int g = 0; g < st.gapAfter && n < MAX_PATH_LEN; g++) {
        pathPoints[p][n++] = { VIRTUAL_OUTPUT, 0 };
      }
    }
  }
  pathLen[p] = n;
}

void rebuildAllZonePaths() {
  for (int p = 0; p < MAX_PATHS; p++) rebuildZonePath(p);
}

// Parses steps text from the page: "zone:reverse:gap,..." e.g. "0:0:0,2:1:3".
// Returns an error message or nullptr.
const char* parseSteps(const String& text, PathStep* steps, uint8_t& count) {
  count = 0;
  int pos = 0, len = text.length();
  while (pos < len) {
    int comma = text.indexOf(',', pos);
    if (comma < 0) comma = len;
    String item = text.substring(pos, comma);
    item.trim();
    pos = comma + 1;
    if (item.length() == 0) continue;
    if (count >= MAX_PATH_STEPS) return "too many zones in the path (max 12)";

    int c1 = item.indexOf(':'), c2 = item.indexOf(':', c1 + 1);
    if (c1 < 0 || c2 < 0) return "bad step format";
    int zone = item.substring(0, c1).toInt();
    int rev  = item.substring(c1 + 1, c2).toInt();
    int gap  = item.substring(c2 + 1).toInt();
    if (zone < 0 || zone >= MAX_ZONES || !zones[zone].used) return "a step uses a zone that doesn't exist";
    steps[count++] = { (uint8_t)zone, (uint8_t)(rev ? 1 : 0),
                       (uint8_t)constrain(gap, 0, MAX_PATH_GAP) };
  }
  if (count == 0) return "add at least one zone to the path";
  return nullptr;
}

// id < 0 = new path. On success, newId is the path's slot.
const char* saveZonePath(int id, const String& name, const String& stepsText, int& newId) {
  PathStep steps[MAX_PATH_STEPS];
  uint8_t count;
  const char* err = parseSteps(stepsText, steps, count);
  if (err) return err;

  char cleanName[NAME_LEN];
  if (!copyName(name, cleanName)) return "enter a path name";

  if (id < 0) {
    for (int p = 0; p < MAX_PATHS && id < 0; p++) if (!pathDefs[p].used) id = p;
    if (id < 0) return "all 8 paths are used; delete one first";
    states[CH_PATH0 + id] = PATH_DEFAULT_STATE;
  } else if (id >= MAX_PATHS || !pathDefs[id].used) {
    return "path not found";
  }

  PathDef& pd = pathDefs[id];
  pd.used = true;
  strcpy(pd.name, cleanName);
  pd.stepCount = count;
  for (int s = 0; s < count; s++) pd.steps[s] = steps[s];
  rebuildZonePath(id);
  resetPhase(CH_PATH0 + id);
  markStateChanged();
  newId = id;
  return nullptr;
}

const char* deleteZonePath(int id) {
  if (id < 0 || id >= MAX_PATHS || !pathDefs[id].used) return "path not found";
  pathDefs[id].used = false;
  pathDefs[id].name[0] = 0;
  pathDefs[id].stepCount = 0;
  states[CH_PATH0 + id] = PATH_DEFAULT_STATE;
  forgetChannel(CH_PATH0 + id, PATH_DEFAULT_STATE);   // scenes forget it (slot reuse)
  rebuildZonePath(id);
  markStateChanged();
  return nullptr;
}

// "Right Side → Front House (reversed) → gap 3 → Porch"
String pathStepsText(int p) {
  String s;
  for (int i = 0; i < pathDefs[p].stepCount; i++) {
    const PathStep& st = pathDefs[p].steps[i];
    if (i) s += " → ";
    s += (st.zone < MAX_ZONES && zones[st.zone].used) ? zones[st.zone].name : "(deleted zone)";
    if (st.reverse) s += " (reversed)";
    if (st.gapAfter && i < pathDefs[p].stepCount - 1) {
      s += " → gap ";
      s += st.gapAfter;
    }
  }
  return s;
}

void loadZonePaths() {
  if (!storageOk || !FFat.exists(PATHS_FILE)) return;
  File f = FFat.open(PATHS_FILE, "r");
  if (!f) return;
  FileHeader h;
  bool okHeader = f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == PATHS_MAGIC;
  bool isV1 = okHeader && h.version == 1 && h.recordSize == sizeof(PathDef) + sizeof(LightStateV1);
  bool isV2 = okHeader && h.version == PATHS_VERSION && h.recordSize == sizeof(PathDef) + sizeof(LightState);
  if (!isV1 && !isV2) {
    Serial.println("Paths: file is from a different firmware layout, starting with no paths");
    f.close();
    return;
  }
  int loaded = 0;
  for (int p = 0; p < h.count && p < MAX_PATHS; p++) {
    if (f.read((uint8_t*)&pathDefs[p], sizeof(PathDef)) != sizeof(PathDef)) break;
    if (isV1) {
      LightStateV1 old;
      if (f.read((uint8_t*)&old, sizeof(old)) != sizeof(old)) break;
      states[CH_PATH0 + p] = upgradeState(old);
    } else {
      if (f.read((uint8_t*)&states[CH_PATH0 + p], sizeof(LightState)) != sizeof(LightState)) break;
    }
    sanitizeState(states[CH_PATH0 + p]);
    pathDefs[p].name[NAME_LEN - 1] = 0;
    if (pathDefs[p].stepCount > MAX_PATH_STEPS) pathDefs[p].used = false;
    if (pathDefs[p].used) loaded++;
  }
  f.close();
  rebuildAllZonePaths();
  Serial.printf("Paths: %d loaded%s\n", loaded, isV1 ? " (converted from the old format)" : "");
  if (isV1) markStateChanged();   // write it back in the new format
  for (int p = 0; p < MAX_PATHS; p++) {
    if (!pathDefs[p].used) continue;
    Serial.printf("  %s = %s (%u positions), %s\n", pathDefs[p].name, pathStepsText(p).c_str(),
                  pathLen[p], states[CH_PATH0 + p].on ? "ON" : "OFF");
  }
}

// Writes all paths and their settings. Called from handleStateSave() (delayed save).
void saveZonePathsFile() {
  if (!storageOk) return;
  File f = FFat.open(PATHS_FILE, "w");
  if (!f) {
    Serial.println("Paths: could not write file");
    return;
  }
  FileHeader h = { PATHS_MAGIC, PATHS_VERSION,
                   (uint16_t)(sizeof(PathDef) + sizeof(LightState)), MAX_PATHS };
  f.write((const uint8_t*)&h, sizeof(h));
  for (int p = 0; p < MAX_PATHS; p++) {
    f.write((const uint8_t*)&pathDefs[p], sizeof(PathDef));
    f.write((const uint8_t*)&states[CH_PATH0 + p], sizeof(LightState));
  }
  f.close();
}
