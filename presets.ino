// presets — saved looks: a name plus a copy of a channel's settings (everything except
// on/off). Saved to /presets.bin. Applying a look to a strand, zone or Whole house
// copies the settings in and turns it on.
//
// Example: "Xmas" = chase, red, background on, green.

const char*    PRESETS_FILE    = "/presets.bin";
const uint32_t PRESETS_MAGIC   = 0x54455250;  // "PRET"
const uint16_t PRESETS_VERSION = 1;

// Saves channel ch's current settings as `name`. A look with the same name is replaced.
const char* savePreset(const String& name, int ch, int& newId) {
  char cleanName[NAME_LEN];
  if (!copyName(name, cleanName)) return "enter a name for the look";

  int id = -1;
  for (int p = 0; p < MAX_PRESETS && id < 0; p++) {
    if (presets[p].used && strcasecmp(presets[p].name, cleanName) == 0) id = p;
  }
  for (int p = 0; p < MAX_PRESETS && id < 0; p++) {
    if (!presets[p].used) id = p;
  }
  if (id < 0) return "all 32 saved looks are used; delete one first";

  presets[id].used = true;
  strcpy(presets[id].name, cleanName);
  presets[id].look = states[ch];
  presets[id].look.on = true;
  savePresetsFile();
  newId = id;
  return nullptr;
}

// Copies look `id` into channel ch and turns it on.
const char* applyPreset(int id, int ch) {
  if (id < 0 || id >= MAX_PRESETS || !presets[id].used) return "saved look not found";
  states[ch] = presets[id].look;
  states[ch].on = true;
  resetPhase(ch);
  markStateChanged();
  return nullptr;
}

const char* deletePreset(int id) {
  if (id < 0 || id >= MAX_PRESETS || !presets[id].used) return "saved look not found";
  presets[id].used = false;
  presets[id].name[0] = 0;
  savePresetsFile();
  return nullptr;
}

void loadPresets() {
  for (int p = 0; p < MAX_PRESETS; p++) {
    presets[p].used = false;
    presets[p].name[0] = 0;
  }
  if (!storageOk || !FFat.exists(PRESETS_FILE)) return;
  File f = FFat.open(PRESETS_FILE, "r");
  if (!f) return;
  FileHeader h;
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h) || h.magic != PRESETS_MAGIC ||
      h.version != PRESETS_VERSION || h.recordSize != sizeof(Preset)) {
    Serial.println("Saved looks: file is from a different firmware layout, starting empty");
    f.close();
    return;
  }
  int loaded = 0;
  for (int p = 0; p < h.count && p < MAX_PRESETS; p++) {
    if (f.read((uint8_t*)&presets[p], sizeof(Preset)) != sizeof(Preset)) break;
    presets[p].name[NAME_LEN - 1] = 0;
    LightState& s = presets[p].look;
    if (s.mode >= MODE_COUNT) s.mode = MODE_SOLID;
    s.fadeSeconds = constrain(s.fadeSeconds, FADE_SECONDS_MIN, FADE_SECONDS_MAX);
    s.speed = constrain(s.speed, SPEED_MIN, SPEED_MAX);
    s.size = constrain(s.size, SIZE_MIN, SIZE_MAX);
    if (presets[p].used) loaded++;
  }
  f.close();
  Serial.printf("Saved looks: %d loaded\n", loaded);
}

void savePresetsFile() {
  if (!storageOk) return;
  File f = FFat.open(PRESETS_FILE, "w");
  if (!f) {
    Serial.println("Saved looks: could not write file");
    return;
  }
  FileHeader h = { PRESETS_MAGIC, PRESETS_VERSION, (uint16_t)sizeof(Preset), MAX_PRESETS };
  f.write((const uint8_t*)&h, sizeof(h));
  for (int p = 0; p < MAX_PRESETS; p++) f.write((const uint8_t*)&presets[p], sizeof(Preset));
  f.close();
}
