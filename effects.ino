// effects — draws each output's mode into frame[] and sends it, at a fixed frame rate.

const uint32_t FRAME_MS = 20;      // 50 frames per second

// Fade phase per output, 0-65535 = one full breath. Advanced by elapsed time each frame,
// so changing the speed slider changes the rate smoothly instead of jumping.
uint16_t fadePhase[NUM_OUTPUTS] = { 0 };

// Makes every output's fade line up with output A (used when both strands are changed together).
void syncFadePhases() {
  for (int o = 1; o < NUM_OUTPUTS; o++) fadePhase[o] = fadePhase[0];
}

// Returns the fade level (fadeMin..255) for output o and advances its phase by dtMs.
// Note: a low fadeMin (under ~20%) can show visible steps at the dim end — each
// 8-bit step is a big jump down there.
uint8_t fadeLevel(int o, uint32_t dtMs) {
  const LightState& s = states[o];
  uint32_t periodMs = (uint32_t)s.fadeSeconds * 1000;
  fadePhase[o] += (uint16_t)((dtMs * 65536UL) / periodMs);
  // sin8 gives 0-255; start each breath at the bottom (phase offset of 3/4 turn)
  uint8_t wave = sin8((uint8_t)((fadePhase[o] >> 8) + 192));
  return s.fadeMin + scale8(wave, 255 - s.fadeMin);
}

// Color for one whole output this frame.
CRGB outputColor(int o, uint32_t dtMs) {
  const LightState& s = states[o];
  CRGB c = CRGB(0, 0, 0);
  if (!s.on) return c;

  switch (s.mode) {
    case MODE_SOLID:
      // nscale8_video: a channel that is on never rounds down to 0 (no color shift when dim)
      c = s.color;
      c.nscale8_video(s.brightness);
      break;
    case MODE_FADE:
      c = s.color;
      c.nscale8_video(scale8(s.brightness, fadeLevel(o, dtMs)));
      break;
    case MODE_TEST:
      c = testPatternColor();
      break;
  }
  return c;
}

void renderFrame() {
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  if (now - lastFrameMs < FRAME_MS) return;
  uint32_t dtMs = now - lastFrameMs;
  lastFrameMs = now;

  for (int o = 0; o < NUM_OUTPUTS; o++) {
    fill_solid(frame[o], NUM_LEDS, outputColor(o, dtMs));
  }
  showFrame();
}
