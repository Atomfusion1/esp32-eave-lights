// effects — draws the current mode into frame[] and sends it, at a fixed frame rate.

const uint32_t FRAME_MS = 20;      // 50 frames per second
const uint8_t  FADE_BPM = 12;      // fade mode: one breath every 5 seconds
const uint8_t  FADE_MIN = 10;      // fade mode: lowest level (0-255), so it never goes fully dark

void renderFrame() {
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  if (now - lastFrameMs < FRAME_MS) return;
  lastFrameMs = now;

  CRGB c = CRGB(0, 0, 0);
  if (state.on) {
    switch (state.mode) {
      case MODE_SOLID:
        c = state.color;
        c.nscale8(state.brightness);
        break;
      case MODE_FADE:
        c = state.color;
        c.nscale8(scale8(state.brightness, beatsin8(FADE_BPM, FADE_MIN, 255)));
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
