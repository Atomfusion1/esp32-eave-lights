// effects — draws the current mode into frame[] and sends it, at a fixed frame rate.

const uint32_t FRAME_MS = 20;      // 50 frames per second

// Fade phase, 0-65535 = one full breath. Advanced by elapsed time each frame, so
// changing the speed slider changes the rate smoothly instead of jumping.
uint16_t fadePhase = 0;

// Returns the fade level (state.fadeMin..255) and advances the phase by dtMs.
// Note: a low fadeMin (under ~20%) can show visible steps at the dim end — each
// 8-bit step is a big jump down there.
uint8_t fadeLevel(uint32_t dtMs) {
  uint32_t periodMs = (uint32_t)state.fadeSeconds * 1000;
  fadePhase += (uint16_t)((dtMs * 65536UL) / periodMs);
  // sin8 gives 0-255; start each breath at the bottom (phase offset of 3/4 turn)
  uint8_t wave = sin8((uint8_t)((fadePhase >> 8) + 192));
  return state.fadeMin + scale8(wave, 255 - state.fadeMin);
}

void renderFrame() {
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  if (now - lastFrameMs < FRAME_MS) return;
  uint32_t dtMs = now - lastFrameMs;
  lastFrameMs = now;

  CRGB c = CRGB(0, 0, 0);
  if (state.on) {
    switch (state.mode) {
      case MODE_SOLID:
        // nscale8_video: a channel that is on never rounds down to 0 (no color shift when dim)
        c = state.color;
        c.nscale8_video(state.brightness);
        break;
      case MODE_FADE:
        c = state.color;
        c.nscale8_video(scale8(state.brightness, fadeLevel(dtMs)));
        break;
      case MODE_TEST:
        c = testPatternColor();
        break;
    }
  }

  for (int o = 0; o < NUM_OUTPUTS; o++) {
    fill_solid(frame[o], NUM_LEDS, c);
  }
  showFrame();
}
