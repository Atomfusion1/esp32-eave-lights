// settings_store — keeps each output's light state in flash
// (ESP32 Preferences / NVS), so the lights come back the way they were after a
// power cut or reboot. Saves are delayed SAVE_DELAY_MS after the last change, so
// dragging a slider doesn't write flash dozens of times.
//
// Keys per output: "<a|b>_on", "_col", "_bri", "_mode", "_fsec", "_fmin", "_spd", "_size", "_rev".
// Versions before 0.4.0 used one set of keys without a prefix ("on", "color", ...);
// those are used as the defaults, so both strands start from the old saved state.

Preferences prefs;
const char*    PREFS_NAMESPACE = "eave";
const uint32_t SAVE_DELAY_MS   = 2000;

bool     stateDirty   = false;
uint32_t stateDirtyMs = 0;

uint32_t colorToRgb(CRGB c) {
  return ((uint32_t)c.r << 16) | ((uint32_t)c.g << 8) | c.b;
}

// Builds "a_on" etc. for output o.
const char* outputKey(int o, const char* name) {
  static char key[16];
  snprintf(key, sizeof(key), "%c_%s", 'a' + o, name);
  return key;
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

  for (int o = 0; o < NUM_OUTPUTS; o++) {
    LightState& s = states[o];
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
  }
  prefs.end();

  Serial.println("State loaded:");
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    const LightState& s = states[o];
    Serial.printf("  %c: %s, #%02X%02X%02X, brightness %u, mode %u, fade %us min %u, "
                  "speed %u size %u %s\n",
                  OUTPUT_LETTERS[o], s.on ? "ON" : "OFF", s.color.r, s.color.g, s.color.b,
                  s.brightness, s.mode, s.fadeSeconds, s.fadeMin,
                  s.speed, s.size, s.reverse ? "reverse" : "forward");
  }
}

// Call after changing `states`; the save happens a little later in handleStateSave().
void markStateChanged() {
  stateDirty = true;
  stateDirtyMs = millis();
}

void handleStateSave() {
  if (!stateDirty || millis() - stateDirtyMs < SAVE_DELAY_MS) return;
  stateDirty = false;

  prefs.begin(PREFS_NAMESPACE, false);
  for (int o = 0; o < NUM_OUTPUTS; o++) {
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
  }
  prefs.end();
  Serial.println("State saved");
}
