// scenes - whole-house snapshots: every channel's settings (strands, Whole house, all
// zones, all zone paths, including on/off) under a name, e.g. "Xmas". Saved to
// /scenes.bin. Applying a scene restores everything at once.
//
// Zone and path definitions (which lights they use) are not stored in a scene; all
// scenes share them. A scene's settings for an unused zone/path slot are ignored, and
// are cleared when a zone/path is deleted (forgetChannel). Paintings and animations are
// shared too, but the page never edits one a saved scene uses: it edits a copy.

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

// ---- Who uses a painting / animation ----

// kind 0 = painting idx, 1 = animation idx (both 0-based).
bool stateUses(int kind, int idx, const LightState& s) {
  if (s.mode == MODE_ANIM) {
    int a = (int)s.size - 1;
    return kind == 1 ? a == idx : animUsesPainting(a, idx);
  }
  return kind == 0 && s.mode == MODE_CUSTOM && (int)s.size - 1 == idx;
}

// True if a saved scene (a channel that is on in it) or a saved look uses it.
bool savedUses(int kind, int idx) {
  for (int sc = 0; sc < MAX_SCENES; sc++) {
    if (!scenes[sc].used) continue;
    for (int ch = 0; ch < NUM_CHANNELS; ch++)
      if (channelInUse(ch) && scenes[sc].states[ch].on && stateUses(kind, idx, scenes[sc].states[ch])) return true;
  }
  for (int p = 0; p < MAX_PRESETS; p++)
    if (presets[p].used && stateUses(kind, idx, presets[p].look)) return true;
  return false;
}

void addUser(String& list, const String& item) {
  String q = "\"" + item + "\"";
  if (list.indexOf(q) >= 0) return;
  if (list.length() > 1) list += ",";
  list += q;
}

// JSON list of what uses painting/animation idx: ["Scene: Xmas","Look: Flag",...].
// locked = a saved scene or look uses it (directly, or through an animation), so the
// page edits a copy instead. withLive = also list animations and what the lights are
// set to now (for deleting).
String lookUsers(int kind, int idx, bool& locked, bool withLive) {
  String list = "[";
  locked = false;
  for (int sc = 0; sc < MAX_SCENES; sc++) {
    if (!scenes[sc].used) continue;
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      if (channelInUse(ch) && scenes[sc].states[ch].on && stateUses(kind, idx, scenes[sc].states[ch])) {
        addUser(list, String("Scene: ") + scenes[sc].name);
        locked = true;
        break;
      }
    }
  }
  for (int p = 0; p < MAX_PRESETS; p++) {
    if (presets[p].used && stateUses(kind, idx, presets[p].look)) {
      addUser(list, String("Look: ") + presets[p].name);
      locked = true;
    }
  }
  if (kind == 0) {
    for (int a = 0; a < MAX_ANIMS; a++) {
      if (!animExists(a) || !animUsesPainting(a, idx)) continue;
      bool saved = savedUses(1, a);   // the animation is in a saved scene/look
      if (saved) locked = true;
      if (withLive || saved) addUser(list, String("Animation: ") + anims[a].name);
    }
  }
  if (withLive) {
    for (int ch = 0; ch < NUM_CHANNELS; ch++)
      if (channelInUse(ch) && stateUses(kind, idx, states[ch])) addUser(list, String("Now set on: ") + channelName(ch));
  }
  return list + "]";
}

// ["A","B"] -> A, B
String listText(const String& json) {
  String t = json.substring(1, json.length() - 1);
  t.replace("\",\"", ", ");
  t.replace("\"", "");
  return t;
}

// A deleted zone/path slot: scenes forget their settings for it, so a new zone or path
// that later gets the same slot doesn't inherit them. Also leaves "All on".
void forgetChannel(int ch, const LightState& def) {
  bool changed = false;
  for (int sc = 0; sc < MAX_SCENES; sc++) {
    if (!scenes[sc].used) continue;
    scenes[sc].states[ch] = def;
    changed = true;
  }
  if (changed) saveScenesFile();
  allOnMask &= ~(1UL << ch);
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

// save = false when a playlist switches scenes: no flash write on every swap (the
// playlist decides what shows anyway).
const char* applyScene(int id, bool save) {
  if (id < 0 || id >= MAX_SCENES || !scenes[id].used) return "scene not found";
  stopLocate();
  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    if (!channelInUse(ch)) continue;
    states[ch] = scenes[id].states[ch];
    resetPhase(ch);
  }
  if (save) markStateChanged();
  Serial.printf("Scene: %s\n", scenes[id].name);
  return nullptr;
}

const char* deleteScene(int id) {
  if (id < 0 || id >= MAX_SCENES || !scenes[id].used) return "scene not found";
  scenes[id].used = false;
  scenes[id].name[0] = 0;
  saveScenesFile();
  // Playlists drop their steps for it, so a new scene in this slot isn't played by them.
  bool changed = false;
  for (int p = 0; p < MAX_PROGRAMS; p++) {
    Program& pr = programs[p];
    if (!pr.used) continue;
    int n = 0;
    for (int k = 0; k < pr.stepCount; k++) {
      if (pr.steps[k].scene == id) { changed = true; continue; }
      pr.steps[n++] = pr.steps[k];
    }
    pr.stepCount = n;
  }
  if (changed) {
    saveProgramsFile();
    nudgeSchedule();
  }
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
