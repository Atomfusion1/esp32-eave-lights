// effects — draws each channel's mode into frame[] and sends it, at a fixed frame rate.
//
// A channel draws onto a "line" of lights: a strand (65 lights) or a path (e.g. the
// 130-light Whole house path). lineLength()/lineLight() map line positions to
// physical lights, so every effect works the same on strands and paths.
//
// Every effect depends only on its settings and elapsed time (no randomness), so a
// browser simulation can reproduce it exactly later.

const uint32_t FRAME_MS = 20;      // 50 frames per second

// Fade phase per channel, 0-65535 = one full breath. Advanced by elapsed time each frame,
// so changing the speed slider changes the rate smoothly instead of jumping.
uint16_t fadePhase[NUM_CHANNELS] = { 0 };

// Chase/comet position per channel, in 1/256ths of a light (fixed point 24.8).
uint32_t motionPos[NUM_CHANNELS] = { 0 };

// Makes both strands' fade and movement line up with strand A
// (used when both strands are changed together).
void syncPhases() {
  for (int o = 1; o < NUM_OUTPUTS; o++) {
    fadePhase[o] = fadePhase[0];
    motionPos[o] = motionPos[0];
  }
}

// Restarts a channel's effect from the beginning (e.g. comet back at the first light).
void resetPhase(int ch) {
  fadePhase[ch] = 0;
  motionPos[ch] = 0;
}

int lineLength(int ch) {
  return ch < NUM_OUTPUTS ? NUM_LEDS : HOUSE_PATH_LEN;
}

// The physical light at position i of channel ch's line.
CRGB& lineLight(int ch, int i) {
  if (ch < NUM_OUTPUTS) return frame[ch][i];
  const PathPoint& p = housePath[i];
  return frame[p.output][p.index];
}

void fillLine(int ch, CRGB c) {
  int len = lineLength(ch);
  for (int i = 0; i < len; i++) lineLight(ch, i) = c;
}

// Returns the fade level (fadeMin..255) for channel ch and advances its phase by dtMs.
// Note: a low fadeMin (under ~20%) can show visible steps at the dim end — each
// 8-bit step is a big jump down there.
uint8_t fadeLevel(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  uint32_t periodMs = (uint32_t)s.fadeSeconds * 1000;
  fadePhase[ch] += (uint16_t)((dtMs * 65536UL) / periodMs);
  // sin8 gives 0-255; start each breath at the bottom (phase offset of 3/4 turn)
  uint8_t wave = sin8((uint8_t)((fadePhase[ch] >> 8) + 192));
  return s.fadeMin + scale8(wave, 255 - s.fadeMin);
}

// Advances channel ch's movement by dtMs and wraps it at `period` lights.
// Returns the position in 1/256ths of a light.
uint32_t advanceMotion(int ch, uint32_t dtMs, uint32_t period) {
  uint32_t period256 = period * 256;
  motionPos[ch] = (motionPos[ch] + (uint32_t)states[ch].speed * dtMs * 256 / 1000) % period256;
  return motionPos[ch];
}

// Sets line position i of channel ch for a moving effect. level 0-255 is how much of
// the effect color shows: 255 = full color, 0 = background (or off if background is off).
// In between, the color blends into the background (comet tail).
void setLight(int ch, int i, uint8_t level) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  // reverse: position 0 of the effect is the last light on the line
  CRGB& light = lineLight(ch, s.reverse ? (len - 1 - i) : i);

  if (!s.bgOn) {
    if (level == 0) {
      light = CRGB(0, 0, 0);
      return;
    }
    CRGB c = s.color;
    c.nscale8_video(scale8(s.brightness, level));
    light = c;
    return;
  }

  CRGB fg = s.color;
  fg.nscale8_video(s.brightness);
  CRGB bg = s.bgColor;
  bg.nscale8_video(s.brightness);
  light = blend(bg, fg, level);
}

// Chase: blocks of `size` lit lights, then `size` dark, repeating and moving.
void drawChase(int ch, uint32_t dtMs) {
  int len = lineLength(ch);
  uint32_t size = states[ch].size;
  uint32_t period = size * 2;
  uint32_t pos = advanceMotion(ch, dtMs, period) >> 8;  // whole lights
  for (int i = 0; i < len; i++) {
    // position of light i inside the repeating pattern, moving forward over time
    uint32_t k = (i + period - pos % period) % period;
    setLight(ch, i, k < size ? 255 : 0);
  }
}

// Comet: bright head with a tail that fades out behind it. The head travels the whole
// line plus the tail length, so the comet fully leaves the end before it restarts.
void drawComet(int ch, uint32_t dtMs) {
  int len = lineLength(ch);
  int32_t tail256 = (int32_t)states[ch].size * 256;
  int32_t head = (int32_t)advanceMotion(ch, dtMs, len + states[ch].size);  // 1/256 lights
  for (int i = 0; i < len; i++) {
    int32_t d = head - (int32_t)i * 256;   // how far light i is behind the head
    uint8_t level = 0;
    if (d >= 0 && d < 256) {
      level = 255;                                     // head
    } else if (d >= 256 && d < tail256 + 256) {
      // tail: falls off with the square of distance, so it looks like it fades smoothly
      uint8_t t = 255 - (uint8_t)(((d - 256) * 255) / tail256);
      level = scale8(t, t);
    }
    setLight(ch, i, level);
  }
}

void drawChannel(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  if (!s.on) {
    fillLine(ch, CRGB(0, 0, 0));
    return;
  }

  CRGB c;
  switch (s.mode) {
    case MODE_SOLID:
      // nscale8_video: a channel that is on never rounds down to 0 (no color shift when dim)
      c = s.color;
      c.nscale8_video(s.brightness);
      fillLine(ch, c);
      break;
    case MODE_FADE:
      c = s.color;
      c.nscale8_video(scale8(s.brightness, fadeLevel(ch, dtMs)));
      fillLine(ch, c);
      break;
    case MODE_TEST:
      fillLine(ch, testPatternColor());
      break;
    case MODE_CHASE:
      drawChase(ch, dtMs);
      break;
    case MODE_COMET:
      drawComet(ch, dtMs);
      break;
  }
}

void renderFrame() {
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  if (now - lastFrameMs < FRAME_MS) return;
  uint32_t dtMs = now - lastFrameMs;
  lastFrameMs = now;

  // Strands first, then the house path on top while it is on.
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    drawChannel(o, dtMs);
  }
  if (states[CH_HOUSE].on) {
    drawChannel(CH_HOUSE, dtMs);
  }
  showFrame();
}
