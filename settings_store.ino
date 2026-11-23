// settings_store — keeps the light state in flash (ESP32 Preferences / NVS),
// so the lights come back the way they were after a power cut or reboot.
// Saves are delayed SAVE_DELAY_MS after the last change, so dragging a slider
// doesn't write flash dozens of times.

Preferences prefs;
const char*    PREFS_NAMESPACE = "eave";
const uint32_t SAVE_DELAY_MS   = 2000;

bool     stateDirty   = false;
uint32_t stateDirtyMs = 0;

void loadState() {
  prefs.begin(PREFS_NAMESPACE, true);  // read-only
  state.on         = prefs.getBool("on", state.on);
  uint32_t rgb     = prefs.getUInt("color", ((uint32_t)state.color.r << 16) |
                                            ((uint32_t)state.color.g << 8) | state.color.b);
  state.color      = CRGB(rgb);
  state.brightness = prefs.getUChar("bri", state.brightness);
  uint8_t m        = prefs.getUChar("mode", state.mode);
  state.mode       = (m < MODE_COUNT) ? (Mode)m : MODE_SOLID;
  prefs.end();

  Serial.printf("State loaded: %s, #%02X%02X%02X, brightness %u, mode %u\n",
                state.on ? "ON" : "OFF", state.color.r, state.color.g, state.color.b,
                state.brightness, state.mode);
}

// Call after changing `state`; the save happens a little later in handleStateSave().
void markStateChanged() {
  stateDirty = true;
  stateDirtyMs = millis();
}

void handleStateSave() {
  if (!stateDirty || millis() - stateDirtyMs < SAVE_DELAY_MS) return;
  stateDirty = false;

  prefs.begin(PREFS_NAMESPACE, false);
  prefs.putBool("on", state.on);
  prefs.putUInt("color", ((uint32_t)state.color.r << 16) |
                         ((uint32_t)state.color.g << 8) | state.color.b);
  prefs.putUChar("bri", state.brightness);
  prefs.putUChar("mode", state.mode);
  prefs.end();
  Serial.println("State saved");
}
