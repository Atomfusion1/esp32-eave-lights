// scenes - whole-house snapshots: every channel's settings (strands, Whole house, all
// zones, all zone paths, including on/off) under a name, e.g. "Xmas". Saved to
// /scenes.bin. Applying a scene restores everything at once.
//
// Zone and path definitions (which lights they use) are not stored in a scene; all
// scenes share them. A scene's settings for an unused zone/path slot are ignored.

const char*    SCENES_FILE    = "/scenes.bin";
const uint32_t SCENES_MAGIC   = 0x4E454353;  // "SCEN"
const uint16_t SCENES_VERSION = 2;   // v1 (before 0.15.0): settings without the color list

// Version 1 record, only used to convert old files.
struct SceneV1 {
  bool         used;
  char         name[NAME_LEN];
  LightStateV1 states[NUM_CHANNELS];
};

bool channelInUse(int ch) {
  if (ch < CH_ZONE0) return true;
  if (ch < CH_PATH0) return zones[ch - CH_ZONE0].used;
  return pathDefs[ch - CH_PATH0].used;
}

// Saves the current settings of every channel as `name`. Same name replaces it.
const char* saveScene(const String& name) {
  char cleanName[NAME_LEN];
  if (!copyName(name, cleanName)) return "enter a name for the scene";

  int id = -1;
  for (int s = 0; s < MAX_SCENES && id < 0; s++) {
    if (scenes[s].used && strcasecmp(scenes[s].name, cleanName) == 0) id = s;
  }
  for (int s = 0; s < MAX_SCENES && id < 0; s++) {
    if (!scenes[s].used) id = s;
  }
  if (id < 0) return "all 16 scenes are used; delete one first";

  scenes[id].used = true;
  strcpy(scenes[id].name, cleanName);
  for (int ch = 0; ch < NUM_CHANNELS; ch++) scenes[id].states[ch] = states[ch];
  saveScenesFile();
  return nullptr;
}

const char* applyScene(int id) {
  if (id < 0 || id >= MAX_SCENES || !scenes[id].used) return "scene not found";
  stopLocate();
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    if (!channelInUse(ch)) continue;
    states[ch] = scenes[id].states[ch];
    resetPhase(ch);
  }
  markStateChanged();
  Serial.printf("Scene: %s\n", scenes[id].name);
  return nullptr;
}

const char* deleteScene(int id) {
  if (id < 0 || id >= MAX_SCENES || !scenes[id].used) return "scene not found";
  scenes[id].used = false;
  scenes[id].name[0] = 0;
  saveScenesFile();
  return nullptr;
}

// {"id","name","on":<channels on>,"colors":["#RRGGBB",...]} for each scene
String scenesJson() {
  String json = "[";
  bool first = true;
  for (int s = 0; s < MAX_SCENES; s++) {
    if (!scenes[s].used) continue;
    if (!first) json += ",";
    first = false;
    int onCount = 0;
    String colors;
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      const LightState& ls = scenes[s].states[ch];
      if (!channelInUse(ch) || !ls.on) continue;
      if (onCount < 4) {
        char c[12];
        snprintf(c, sizeof(c), "%s\"#%02X%02X%02X\"", onCount ? "," : "", ls.color.r, ls.color.g, ls.color.b);
        colors += c;
      }
      onCount++;
    }
    json += "{\"id\":";
    json += s;
    json += ",\"name\":\"";
    json += scenes[s].name;
    json += "\",\"on\":";
    json += onCount;
    json += ",\"colors\":[";
    json += colors;
    json += "]}";
  }
  json += "]";
  return json;
}

void loadScenes() {
  for (int s = 0; s < MAX_SCENES; s++) {
    scenes[s].used = false;
    scenes[s].name[0] = 0;
  }
  if (!storageOk || !FFat.exists(SCENES_FILE)) return;
  File f = FFat.open(SCENES_FILE, "r");
  if (!f) return;
  FileHeader h;
  bool okHeader = f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == SCENES_MAGIC;
  bool isV1 = okHeader && h.version == 1 && h.recordSize == sizeof(SceneV1);
  bool isV2 = okHeader && h.version == SCENES_VERSION && h.recordSize == sizeof(Scene);
  if (!isV1 && !isV2) {
    Serial.println("Scenes: file is from a different firmware layout, starting empty");
    f.close();
    return;
  }
  int loaded = 0;
  for (int s = 0; s < h.count && s < MAX_SCENES; s++) {
    if (isV1) {
      // Read one old scene at a time (heap, it's ~360 bytes) and convert it.
      SceneV1* old = new SceneV1;
      bool ok = f.read((uint8_t*)old, sizeof(SceneV1)) == sizeof(SceneV1);
      if (ok) {
        scenes[s].used = old->used;
        memcpy(scenes[s].name, old->name, NAME_LEN);
        for (int ch = 0; ch < NUM_CHANNELS; ch++) scenes[s].states[ch] = upgradeState(old->states[ch]);
      }
      delete old;
      if (!ok) break;
    } else {
      if (f.read((uint8_t*)&scenes[s], sizeof(Scene)) != sizeof(Scene)) break;
    }
    scenes[s].name[NAME_LEN - 1] = 0;
    for (int ch = 0; ch < NUM_CHANNELS; ch++) sanitizeState(scenes[s].states[ch]);
    if (scenes[s].used) loaded++;
  }
  f.close();
  Serial.printf("Scenes: %d loaded%s\n", loaded, isV1 ? " (converted from the old format)" : "");
  if (isV1) saveScenesFile();   // write it back in the new format
}

void saveScenesFile() {
  if (!storageOk || savesBlocked) return;
  File f = FFat.open(SCENES_FILE, "w");
  if (!f) {
    Serial.println("Scenes: could not write file");
    return;
  }
  FileHeader h = { SCENES_MAGIC, SCENES_VERSION, (uint16_t)sizeof(Scene), MAX_SCENES };
  f.write((const uint8_t*)&h, sizeof(h));
  for (int s = 0; s < MAX_SCENES; s++) f.write((const uint8_t*)&scenes[s], sizeof(Scene));
  f.close();
}
