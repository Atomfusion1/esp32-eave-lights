// settings_store — keeps the strand A, B and Whole house settings in flash (ESP32
// Preferences / NVS), so the lights come back the way they were after a power cut or
// reboot. Zone settings are saved with the zones in /zones.bin (zones.ino).
// Saves are delayed SAVE_DELAY_MS after the last change, so dragging a slider doesn't
// write flash dozens of times.
//
// Also mounts the FAT storage area (the 9.9 MB "ffat" partition) used for zones and
// saved looks.
//
// Keys per channel: "<prefix>_on", "_col", "_bri", "_mode", "_fsec", "_fmin", "_spd",
// "_size", "_rev", "_bgon", "_bgc"; prefix a = strand A, b = strand B, c = Whole house,
// sc..sf = strands C-F (v0.36.0; the older prefixes stay as they were).
// Strand lengths: "<prefix>_cnt". Color list (v0.15.0+): "_ncol" and colors 2-5 "_x0".."_x3".
// "chv" = channel layout of the saved "allon" list (missing = before v0.36.0, 2 strands).
// Versions before 0.4.0 used one set of keys without a prefix ("on", "color", ...);
// those are used as the defaults, so both strands start from the old saved state.
// PREFS_NAMESPACE is in the main tab (led_output.ino uses it too).

Preferences prefs;
const uint32_t SAVE_DELAY_MS   = 2000;
#define CHANNEL_LAYOUT 2   // "chv": 2 = strands A-F (v0.36.0)

// Channels kept in Preferences: strands and Whole house (zones live in /zones.bin).
#define PREFS_CHANNELS CH_ZONE0

bool     stateDirty   = false;
uint32_t stateDirtyMs = 0;

// Converts a pre-0.15.0 saved state; colors 2-5 get the defaults.
LightState upgradeState(const LightStateV1& o) {
  LightState s = { o.on, o.color, o.brightness, o.mode, o.fadeSeconds, o.fadeMin, o.speed, o.size,
                   o.reverse, 0, o.bgOn, 0, 0, o.bgColor, DEFAULT_EXTRA_COLORS };
  return s;
}

// Keeps loaded/received values in range.
void sanitizeState(LightState& s) {
  if (s.mode >= MODE_COUNT) s.mode = MODE_SOLID;
  s.fadeSeconds = constrain(s.fadeSeconds, FADE_SECONDS_MIN, FADE_SECONDS_MAX);
  s.speed = constrain(s.speed, SPEED_MIN, SPEED_MAX);
  s.size = constrain(s.size, SIZE_MIN, SIZE_MAX);
  s.colorCount = constrain(s.colorCount, 1, MAX_COLORS);
  if (s.jitter > 100) s.jitter = 100;
}

uint32_t colorToRgb(CRGB c) {
  return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

// "A".."F", "House", or the zone's / path's name — for Serial messages.
const char* channelName(int ch) {
  static const char* names[CH_ZONE0] = { "A", "B", "C", "D", "E", "F", "House" };
  if (ch < CH_ZONE0) return names[ch];
  if (ch < CH_PATH0) return zones[ch - CH_ZONE0].name;
  return pathDefs[ch - CH_PATH0].name;
}

// Builds "a_on" etc. for channel o: 'a' = strand A, 'b' = strand B, 'c' = Whole house
// (as before v0.36.0), "sc".."sf" = strands C-F.
const char* outputKey(int o, const char* name) {
  static char key[16];
  if (o == CH_HOUSE)          snprintf(key, sizeof(key), "c_%s", name);
  else if (o < HOUSE_STRANDS) snprintf(key, sizeof(key), "%c_%s", 'a' + o, name);
  else                        snprintf(key, sizeof(key), "s%c_%s", 'a' + o, name);
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
    // Strands C-F and the house path are newer than the single-state keys: own defaults.
    if (o >= HOUSE_STRANDS) old = s;
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
    s.jitter      = prefs.getUChar(outputKey(o, "rnd"), s.jitter);
    s.bgOn        = prefs.getBool(outputKey(o, "bgon"), s.bgOn);
    s.bothWays    = prefs.getBool(outputKey(o, "both"), s.bothWays);
    s.flipEvery   = prefs.getUChar(outputKey(o, "flip"), s.flipEvery);
    s.bgColor     = CRGB(prefs.getUInt(outputKey(o, "bgc"), colorToRgb(s.bgColor)));
    s.colorCount  = prefs.getUChar(outputKey(o, "ncol"), s.colorCount);
    for (int k = 0; k < MAX_COLORS - 1; k++) {
      char name[4] = { 'x', (char)('0' + k), 0 };
      s.extra[k] = CRGB(prefs.getUInt(outputKey(o, name), colorToRgb(s.extra[k])));
    }
    sanitizeState(s);
  }
  // Strand lengths ("a_cnt", "b_cnt").
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    ledCount[o] = constrain(prefs.getUShort(outputKey(o, "cnt"), ledCount[o]), 1, MAX_LEDS);
  }
  allOnMask = prefs.getUInt("allon", 0);   // what "All lights on" brings back
  if (prefs.getUChar("chv", 0) < CHANNEL_LAYOUT) {
    // saved before v0.36.0: Whole house, zones and paths moved up by 4 channels
    uint32_t m = 0;
    for (int ch = 0; ch < LEGACY_CHANNELS; ch++) if (allOnMask & (1UL << ch)) m |= 1UL << legacyChannel(ch);
    allOnMask = m;
  }
  scheduleOn = prefs.getBool("sched", false);
  {
    LightTimer t;
    if (prefs.getBytesLength("timer") == sizeof(t) && prefs.getBytes("timer", &t, sizeof(t)) == sizeof(t)) lightTimer = t;
  }
  manualProgram = prefs.getChar("mprog", -1);
  fallbackProgram = prefs.getChar("fallback", FALLBACK_KEEP);
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

// Saves a pending change now (e.g. before a restart).
void handleStateSaveNow() {
  if (stateDirty) stateDirtyMs = millis() - SAVE_DELAY_MS;
  handleStateSave();
}

void handleStateSave() {
  if (!stateDirty || millis() - stateDirtyMs < SAVE_DELAY_MS) return;
  if (restoreBlocksSaving()) return;   // a backup is being restored; save afterwards
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
    prefs.putUChar(outputKey(o, "rnd"), s.jitter);
    prefs.putBool(outputKey(o, "bgon"), s.bgOn);
    prefs.putBool(outputKey(o, "both"), s.bothWays);
    prefs.putUChar(outputKey(o, "flip"), s.flipEvery);
    prefs.putUInt(outputKey(o, "bgc"), colorToRgb(s.bgColor));
    prefs.putUChar(outputKey(o, "ncol"), s.colorCount);
    for (int k = 0; k < MAX_COLORS - 1; k++) {
      char name[4] = { 'x', (char)('0' + k), 0 };
      prefs.putUInt(outputKey(o, name), colorToRgb(s.extra[k]));
    }
  }
  for (int o = 0; o < NUM_OUTPUTS; o++) prefs.putUShort(outputKey(o, "cnt"), ledCount[o]);
  prefs.putUInt("allon", allOnMask);
  prefs.putUChar("chv", CHANNEL_LAYOUT);
  prefs.putBool("sched", scheduleOn);
  prefs.putBytes("timer", &lightTimer, sizeof(lightTimer));
  prefs.putChar("mprog", manualProgram);
  prefs.putChar("fallback", fallbackProgram);
  prefs.end();

  // Zone and path settings (on/off, color, ...) live in their files, so write them too.
  saveZonesFile();
  saveZonePathsFile();
  Serial.println("State saved");
}
