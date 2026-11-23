// settings_store — keeps the strand A, B and Whole house settings in flash (ESP32
// Preferences / NVS), so the lights come back the way they were after a power cut or
// reboot. Zone settings are saved with the zones in /zones.bin (zones.ino).
// Saves are delayed SAVE_DELAY_MS after the last change, so dragging a slider doesn't
// write flash dozens of times.
//
// Also mounts the FAT storage area (the 9.9 MB "ffat" partition) used for zones and
// saved looks.
//
// Keys per channel: "<a|b|c>_on", "_col", "_bri", "_mode", "_fsec", "_fmin", "_spd",
// "_size", "_rev", "_bgon", "_bgc" (a = strand A, b = strand B, c = Whole house).
// Strand lengths: "a_cnt", "b_cnt".
// Versions before 0.4.0 used one set of keys without a prefix ("on", "color", ...);
// those are used as the defaults, so both strands start from the old saved state.

Preferences prefs;
const char*    PREFS_NAMESPACE = "eave";
const uint32_t SAVE_DELAY_MS   = 2000;

// Channels kept in Preferences: strands and Whole house (zones live in /zones.bin).
#define PREFS_CHANNELS CH_ZONE0

bool     stateDirty   = false;
uint32_t stateDirtyMs = 0;

uint32_t colorToRgb(CRGB c) {
  return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

// "A", "B", "House", or the zone's / path's name — for Serial messages.
const char* channelName(int ch) {
  static const char* names[CH_ZONE0] = { "A", "B", "House" };
  if (ch < CH_ZONE0) return names[ch];
  if (ch < CH_PATH0) return zones[ch - CH_ZONE0].name;
  return pathDefs[ch - CH_PATH0].name;
}

// Builds "a_on" etc. for channel o ('a' = strand A, 'b' = strand B, 'c' = Whole house).
const char* outputKey(int o, const char* name) {
  static char key[16];
  snprintf(key, sizeof(key), "%c_%s", 'a' + o, name);
  return key;
}

// Mounts the FAT storage area. Formats it the first time (takes a few seconds once).
void startStorage() {
  storageOk = FFat.begin(true);
  if (storageOk) {
    Serial.printf("Storage: %u KB free of %u KB\n",
                  (unsigned)(FFat.freeBytes() / 1024), (unsigned)(FFat.totalBytes() / 1024));
  } else {
    Serial.println("Storage: FAT partition not found. Zones and saved looks won't be kept.");
    Serial.println("         Tools > Partition Scheme > 16M Flash (3MB APP/9.9MB FATFS)");
  }
}

void loadState() {
  prefs.begin(PREFS_NAMESPACE, true);  // read-only

  // Pre-0.4.0 single-state values, used as defaults for every output.
  LightState old = states[0];
  old.on          = prefs.getBool("on", old.on);
  old.color       = CRGB(prefs.getUInt("color", colorToRgb(old.color)));
  old.brightness  = prefs.getUChar("bri", old.brightness);
  uint8_t om      = prefs.getUChar("mode", old.mode);
  old.mode        = (om < MODE_COUNT) ? (Mode)om : MODE_SOLID;
  old.fadeSeconds = prefs.getUChar("fsec", old.fadeSeconds);
  old.fadeMin     = prefs.getUChar("fmin", old.fadeMin);

  for (int o = 0; o < PREFS_CHANNELS; o++) {
    LightState& s = states[o];
    // The house path is newer than the single-state keys: use its own defaults.
    if (o >= NUM_OUTPUTS) old = s;
    s.on          = prefs.getBool(outputKey(o, "on"), old.on);
    s.color       = CRGB(prefs.getUInt(outputKey(o, "col"), colorToRgb(old.color)));
    s.brightness  = prefs.getUChar(outputKey(o, "bri"), old.brightness);
    uint8_t m     = prefs.getUChar(outputKey(o, "mode"), old.mode);
    s.mode        = (m < MODE_COUNT) ? (Mode)m : MODE_SOLID;
    s.fadeSeconds = constrain(prefs.getUChar(outputKey(o, "fsec"), old.fadeSeconds),
                              FADE_SECONDS_MIN, FADE_SECONDS_MAX);
    s.fadeMin     = prefs.getUChar(outputKey(o, "fmin"), old.fadeMin);
    s.speed       = constrain(prefs.getUChar(outputKey(o, "spd"), s.speed), SPEED_MIN, SPEED_MAX);
    s.size        = constrain(prefs.getUChar(outputKey(o, "size"), s.size), SIZE_MIN, SIZE_MAX);
    s.reverse     = prefs.getBool(outputKey(o, "rev"), s.reverse);
    s.bgOn        = prefs.getBool(outputKey(o, "bgon"), s.bgOn);
    s.bgColor     = CRGB(prefs.getUInt(outputKey(o, "bgc"), colorToRgb(s.bgColor)));
  }
  // Strand lengths ("a_cnt", "b_cnt").
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    ledCount[o] = constrain(prefs.getUShort(outputKey(o, "cnt"), ledCount[o]), 1, MAX_LEDS);
  }
  prefs.end();

  Serial.println("State loaded:");
  for (int o = 0; o < PREFS_CHANNELS; o++) {
    const LightState& s = states[o];
    Serial.printf("  %s: %s, #%02X%02X%02X, brightness %u, mode %u, fade %us min %u, "
                  "speed %u size %u %s, background %s #%02X%02X%02X\n",
                  channelName(o), s.on ? "ON" : "OFF", s.color.r, s.color.g, s.color.b,
                  s.brightness, s.mode, s.fadeSeconds, s.fadeMin,
                  s.speed, s.size, s.reverse ? "reverse" : "forward",
                  s.bgOn ? "on" : "off", s.bgColor.r, s.bgColor.g, s.bgColor.b);
  }
}

// Call after changing `states` or zones; the save happens a little later in handleStateSave().
void markStateChanged() {
  stateDirty = true;
  stateDirtyMs = millis();
}

void handleStateSave() {
  if (!stateDirty || millis() - stateDirtyMs < SAVE_DELAY_MS) return;
  stateDirty = false;

  prefs.begin(PREFS_NAMESPACE, false);
  for (int o = 0; o < PREFS_CHANNELS; o++) {
    const LightState& s = states[o];
    prefs.putBool(outputKey(o, "on"), s.on);
    prefs.putUInt(outputKey(o, "col"), colorToRgb(s.color));
    prefs.putUChar(outputKey(o, "bri"), s.brightness);
    prefs.putUChar(outputKey(o, "mode"), s.mode);
    prefs.putUChar(outputKey(o, "fsec"), s.fadeSeconds);
    prefs.putUChar(outputKey(o, "fmin"), s.fadeMin);
    prefs.putUChar(outputKey(o, "spd"), s.speed);
    prefs.putUChar(outputKey(o, "size"), s.size);
    prefs.putBool(outputKey(o, "rev"), s.reverse);
    prefs.putBool(outputKey(o, "bgon"), s.bgOn);
    prefs.putUInt(outputKey(o, "bgc"), colorToRgb(s.bgColor));
  }
  for (int o = 0; o < NUM_OUTPUTS; o++) prefs.putUShort(outputKey(o, "cnt"), ledCount[o]);
  prefs.end();

  // Zone and path settings (on/off, color, ...) live in their files, so write them too.
  saveZonesFile();
  saveZonePathsFile();
  Serial.println("State saved");
}
