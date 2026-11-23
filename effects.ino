// effects — draws each output's mode into frame[] and sends it, at a fixed frame rate.
//
// Every effect depends only on its settings and elapsed time (no randomness), so a
// browser simulation can reproduce it exactly later.

const uint32_t FRAME_MS = 20;      // 50 frames per second

// Fade phase per output, 0-65535 = one full breath. Advanced by elapsed time each frame,
// so changing the speed slider changes the rate smoothly instead of jumping.
uint16_t fadePhase[NUM_OUTPUTS] = { 0 };

// Chase/comet position per output, in 1/256ths of a light (fixed point 24.8).
uint32_t motionPos[NUM_OUTPUTS] = { 0 };

// Makes every output's fade and movement line up with output A
// (used when both strands are changed together).
void syncPhases() {
  for (int o = 1; o < NUM_OUTPUTS; o++) {
    fadePhase[o] = fadePhase[0];
    motionPos[o] = motionPos[0];
  }
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

// Advances output o's movement by dtMs and wraps it at `period` lights.
// Returns the position in 1/256ths of a light.
uint32_t advanceMotion(int o, uint32_t dtMs, uint32_t period) {
  uint32_t period256 = period * 256;
  motionPos[o] = (motionPos[o] + (uint32_t)states[o].speed * dtMs * 256 / 1000) % period256;
  return motionPos[o];
}

// Sets light i of output o to the output's color at `level` (0-255) times its brightness.
void setLight(int o, int i, uint8_t level) {
  const LightState& s = states[o];
  // reverse: light 0 of the effect is the last physical light
  int idx = s.reverse ? (NUM_LEDS - 1 - i) : i;
  if (level == 0) {
    frame[o][idx] = CRGB(0, 0, 0);
    return;
  }
  CRGB c = s.color;
  c.nscale8_video(scale8(s.brightness, level));
  frame[o][idx] = c;
}

// Chase: blocks of `size` lit lights, then `size` dark, repeating and moving.
void drawChase(int o, uint32_t dtMs) {
  uint32_t size = states[o].size;
  uint32_t period = size * 2;
  uint32_t pos = advanceMotion(o, dtMs, period) >> 8;  // whole lights
  for (int i = 0; i < NUM_LEDS; i++) {
    // position of light i inside the repeating pattern, moving forward over time
    uint32_t k = (i + period - pos % period) % period;
    setLight(o, i, k < size ? 255 : 0);
  }
}

// Comet: bright head with a tail that fades out behind it. The head travels the whole
// strand plus the tail length, so the comet fully leaves the end before it restarts.
void drawComet(int o, uint32_t dtMs) {
  int32_t tail256 = (int32_t)states[o].size * 256;
  int32_t head = (int32_t)advanceMotion(o, dtMs, NUM_LEDS + states[o].size);  // 1/256 lights
  for (int i = 0; i < NUM_LEDS; i++) {
    int32_t d = head - (int32_t)i * 256;   // how far light i is behind the head
    uint8_t level = 0;
    if (d >= 0 && d < 256) {
      level = 255;                                     // head
    } else if (d >= 256 && d < tail256 + 256) {
      // tail: falls off with the square of distance, so it looks like it fades smoothly
      uint8_t t = 255 - (uint8_t)(((d - 256) * 255) / tail256);
      level = scale8(t, t);
    }
    setLight(o, i, level);
  }
}

void drawOutput(int o, uint32_t dtMs) {
  const LightState& s = states[o];
  if (!s.on) {
    fill_solid(frame[o], NUM_LEDS, CRGB(0, 0, 0));
    return;
  }

  CRGB c;
  switch (s.mode) {
    case MODE_SOLID:
      // nscale8_video: a channel that is on never rounds down to 0 (no color shift when dim)
      c = s.color;
      c.nscale8_video(s.brightness);
      fill_solid(frame[o], NUM_LEDS, c);
      break;
    case MODE_FADE:
      c = s.color;
      c.nscale8_video(scale8(s.brightness, fadeLevel(o, dtMs)));
      fill_solid(frame[o], NUM_LEDS, c);
      break;
    case MODE_TEST:
      fill_solid(frame[o], NUM_LEDS, testPatternColor());
      break;
    case MODE_CHASE:
      drawChase(o, dtMs);
      break;
    case MODE_COMET:
      drawComet(o, dtMs);
      break;
  }
}

void renderFrame() {
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  if (now - lastFrameMs < FRAME_MS) return;
  uint32_t dtMs = now - lastFrameMs;
  lastFrameMs = now;

  for (int o = 0; o < NUM_OUTPUTS; o++) {
    drawOutput(o, dtMs);
  }
  showFrame();
}
