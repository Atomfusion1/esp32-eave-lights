// effects — draws each channel's mode into frame[] and sends it, at a fixed frame rate.
//
// A channel draws onto a "line" of lights: a strand (ledCount lights) or a path (e.g. the
// 130-light Whole house path). lineLength()/lineLight() map line positions to
// physical lights, so every effect works the same on strands and paths.
//
// Every effect depends only on its settings and elapsed time (no randomness), so a
// browser simulation can reproduce it exactly later. Effects that look random (twinkle,
// sparkle, meteor, lightning) use hash32() of light number and time slot instead.
// Randomness (jitter) also comes from hash32(): see jitterDt().
// The basic effects are here; the rest are in effects_more.ino.

const uint32_t FRAME_MS = 20;      // 50 frames per second

// Fade phase per channel, 0-65535 = one full breath. Advanced by elapsed time each frame,
// so changing the speed slider changes the rate smoothly instead of jumping.
uint16_t fadePhase[NUM_CHANNELS] = { 0 };

// Chase/comet position per channel, in 1/256ths of a light (fixed point 24.8).
uint32_t motionPos[NUM_CHANNELS] = { 0 };

// Random direction state (see advanceMotion): -256 backward .. 256 forward (0 at boot
// ramps up), passes done (comet/meteor), and whether this pass runs the other way.
int16_t  dirRate[NUM_CHANNELS] = { 0 };
uint16_t passNo[NUM_CHANNELS]  = { 0 };
bool     flipNow[NUM_CHANNELS] = { false };

// effectMs[] (time since each channel's effect started) is in the main tab:
// animations.ino comes before this tab and uses it too.

// Makes both strands' fade and movement line up with strand A
// (used when both strands are changed together).
void syncPhases() {
  for (int o = 1; o < HOUSE_STRANDS; o++) {
    fadePhase[o] = fadePhase[0];
    motionPos[o] = motionPos[0];
    effectMs[o]  = effectMs[0];
    dirRate[o]   = dirRate[0];
    passNo[o]    = passNo[0];
  }
}

// Restarts a channel's effect from the beginning (e.g. comet back at the first light).
void resetPhase(int ch) {
  fadePhase[ch] = 0;
  motionPos[ch] = 0;
  effectMs[ch]  = 0;
  dirRate[ch]   = 256;
  passNo[ch]    = 0;
}

int lineLength(int ch) {
  if (ch < NUM_OUTPUTS) return strandLen[ch];   // "never light" zones left out
  if (ch == CH_HOUSE)   return houseLen;
  if (ch < CH_PATH0)    return zoneLen[ch - CH_ZONE0];
  return pathLen[ch - CH_PATH0];
}

// Where gap positions of a path draw to: a light that is never shown.
CRGB gapSink;

// Which physical light is at position i of channel ch's line.
// Returns false for a path gap (no real light there).
bool linePoint(int ch, int i, int& output, int& index) {
  if (ch < NUM_OUTPUTS) { output = ch; index = strandMap[ch][i]; return true; }
  const PathPoint& p = (ch == CH_HOUSE) ? housePath[i]
                     : (ch < CH_PATH0)  ? zonePoints[ch - CH_ZONE0][i]
                                        : pathPoints[ch - CH_PATH0][i];
  if (p.output == VIRTUAL_OUTPUT) return false;
  output = p.output; index = p.index;
  return true;
}

// The physical light at position i of channel ch's line.
CRGB& lineLight(int ch, int i) {
  if (ch < NUM_OUTPUTS) return frame[ch][strandMap[ch][i]];
  const PathPoint& p = (ch == CH_HOUSE) ? housePath[i]
                     : (ch < CH_PATH0)  ? zonePoints[ch - CH_ZONE0][i]
                                        : pathPoints[ch - CH_PATH0][i];
  if (p.output == VIRTUAL_OUTPUT) return gapSink;
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
  uint32_t periodMs = (uint32_t)max((int)s.fadeSeconds, FADE_SECONDS_MIN) * 1000;
  fadePhase[ch] += (uint16_t)((dtMs * 65536UL) / periodMs);
  // sin8 gives 0-255; start each breath at the bottom (phase offset of 3/4 turn)
  uint8_t wave = sin8((uint8_t)((fadePhase[ch] >> 8) + 192));
  return s.fadeMin + scale8(wave, 255 - s.fadeMin);
}

// Random direction (bothWays, v0.35.0).
// Line effects (chase, rainbow wave, wipe, gradient, stripes, center, running): every
// flipEvery x 5 s a coin toss (hash32, repeatable) picks forward or backward, and the
// movement slows, stops and speeds up the other way over about 0.8 s (dirRate).
// Comet / meteor: each new pass starts from a random end (passNo counts passes).
// Leaves pick per leaf (drawLeaves). Strands A and B share one coin, so Both stays in step.

bool lineFlips(uint8_t m) {
  return m == MODE_CHASE || m == MODE_RAINBOW || m == MODE_WIPE || m == MODE_GRADIENT ||
         m == MODE_STRIPES || m == MODE_CENTER || m == MODE_RUNNING;
}
bool passFlips(uint8_t m) { return m == MODE_COMET || m == MODE_METEOR; }

uint32_t flipSeed(int ch) { return (ch < HOUSE_STRANDS) ? 0 : (uint32_t)ch * 0x85EBCA6BUL + 29; }

// Comet/meteor: which way the current pass runs.
void updatePassFlip(int ch) {
  flipNow[ch] = states[ch].bothWays && (hash32(passNo[ch] * 0x27D4EB2DUL ^ flipSeed(ch)) & 1);
}

// Advances channel ch's movement by dtMs and wraps it at `period` lights.
// Returns the position in 1/256ths of a light.
uint32_t advanceMotion(int ch, uint32_t dtMs, uint32_t period) {
  const LightState& s = states[ch];
  int64_t period256 = (int64_t)period * 256;
  if (period256 <= 0) return 0;
  int64_t step = (int64_t)s.speed * dtMs * 256 / 1000;
  if (s.bothWays && lineFlips(s.mode)) {
    uint32_t everyMs = (uint32_t)(s.flipEvery ? s.flipEvery : 6) * 5000UL;
    uint32_t slot = effectMs[ch] / everyMs;
    int target = (slot == 0 || !(hash32(slot * 0x9E3779B1UL ^ flipSeed(ch)) & 1)) ? 256 : -256;
    int r = dirRate[ch], d = (int)(dtMs * 512 / 800) + 1;           // full turn in ~0.8 s
    r = (r < target) ? min(r + d, target) : max(r - d, target);
    dirRate[ch] = r;
    step = step * r / 256;
  } else {
    dirRate[ch] = 256;
  }
  int64_t p = (int64_t)motionPos[ch] + step;
  if (p >= period256) passNo[ch]++;                                   // a new pass begins
  motionPos[ch] = (uint32_t)(((p % period256) + period256) % period256);
  return motionPos[ch];
}

// Sets line position i of channel ch for a moving effect.
// level 1-255: the effect color at that fraction of the brightness (comet tail fades
//              in brightness only and keeps its color — no mixing with the background).
// level 0:     background color if background is on, otherwise off.
void setLight(int ch, int i, uint8_t level) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  // reverse: position 0 of the effect is the last light on the line
  CRGB& light = lineLight(ch, (s.reverse ^ flipNow[ch]) ? (len - 1 - i) : i);

  CRGB c;
  if (level == 0) {
    if (!s.bgOn) {
      light = CRGB(0, 0, 0);
      return;
    }
    c = s.bgColor;
    c.nscale8_video(s.brightness);
  } else {
    c = s.color;
    c.nscale8_video(scale8(s.brightness, level));
  }
  light = c;
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
  updatePassFlip(ch);   // Random direction: this pass may start from the other end
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

// Randomness (LightState.jitter, 0-100 %): the channel's clock runs faster and slower by
// up to that much, wandering smoothly to a new pace every 2.5 s. Every effect is driven
// by dtMs/effectMs, so they all get it. Seeded by channel (strands A and B share one,
// so "Both strands" stays in step); repeatable because the pace comes from hash32().
uint8_t jitterFrac[NUM_CHANNELS] = { 0 };   // leftover 1/256 ms per channel

float jitterPace(int ch, uint32_t k) {      // -1..1 for 2.5-s step k
  uint32_t seed = (ch < HOUSE_STRANDS) ? 0 : (uint32_t)ch * 0x85EBCA6BUL + 11;
  return (hash32(k * 0x9E3779B1UL ^ seed) & 0xFFFF) / 32767.5f - 1.0f;
}

uint32_t jitterDt(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  if (!s.jitter) return dtMs;
  const uint32_t STEP_MS = 2500;
  uint32_t t = effectMs[ch], k = t / STEP_MS;
  float u = (t % STEP_MS) / (float)STEP_MS;
  u = u * u * (3.0f - 2.0f * u);                                  // smooth between steps
  float a = jitterPace(ch, k), b = jitterPace(ch, k + 1);
  float rate = 1.0f + (a + (b - a) * u) * s.jitter * 0.009f;     // 100 % = 0.1x to 1.9x
  uint32_t total = (uint32_t)(dtMs * rate * 256.0f + 0.5f) + jitterFrac[ch];
  jitterFrac[ch] = total & 255;
  return total >> 8;
}

void drawChannel(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  if (!s.on) {
    fillLine(ch, CRGB(0, 0, 0));
    return;
  }

  dtMs = jitterDt(ch, dtMs);   // randomness: this channel's pace wanders
  flipNow[ch] = false;         // set again by comet/meteor for Random direction
  effectMs[ch] += dtMs;

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
    // effects_more.ino
    case MODE_RAINBOW:      drawRainbowWave(ch, dtMs);  break;
    case MODE_RAINBOWCYCLE: drawRainbowCycle(ch);       break;
    case MODE_WIPE:         drawWipe(ch, dtMs);         break;
    case MODE_GRADIENT:     drawGradient(ch, dtMs);     break;
    case MODE_STRIPES:      drawStripes(ch, dtMs);      break;
    case MODE_ALTERNATE:    drawAlternate(ch);          break;
    case MODE_COLORCYCLE:   drawColorCycle(ch);         break;
    case MODE_TWINKLE:      drawTwinkle(ch);            break;
    case MODE_BOUNCE:       drawBounce(ch, dtMs);       break;
    case MODE_CENTER:       drawCenter(ch, dtMs);       break;
    case MODE_METEOR:       drawMeteor(ch, dtMs);       break;
    case MODE_RUNNING:      drawRunning(ch, dtMs);      break;
    case MODE_SPARKLE:      drawSparkle(ch);            break;
    case MODE_FIRE:         drawFire(ch);               break;
    case MODE_LIGHTNING:    drawLightning(ch);          break;
    case MODE_STROBE:       drawStrobe(ch);             break;
    case MODE_CUSTOM:       drawCustom(ch);             break;   // patterns.ino
    case MODE_ANIM:         drawAnim(ch);               break;   // animations.ino
    case MODE_SWAY:         drawSway(ch);               break;
    case MODE_BULBS:        drawBulbs(ch);              break;
    case MODE_GLITTER:      drawGlitter(ch);            break;
    case MODE_BLINKSHIFT:   drawBlinkShift(ch);         break;
    case MODE_LEAVES:       drawLeaves(ch);             break;
  }
}

void renderFrame() {
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  if (now - lastFrameMs < FRAME_MS) return;
  uint32_t dtMs = now - lastFrameMs;
  lastFrameMs = now;

  previewBeforeFrame();
  drawLayers(dtMs);
  showFrame();
  previewAfterFrame(dtMs);   // draws the unapplied-changes preview, if one is active
}

// Draws every layer into frame[]. Layers, later ones cover earlier ones:
// strands, then Whole house while on, then zone paths that are on, then zones that
// are on (e.g. a steady porch stays steady while a comet passes behind it).
void drawLayers(uint32_t dtMs) {
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    if (strandLen[o]) drawChannel(o, dtMs);
  }
  if (states[CH_HOUSE].on && houseLen) {
    drawChannel(CH_HOUSE, dtMs);
  }
  for (int p = 0; p < MAX_PATHS; p++) {
    if (pathDefs[p].used && states[CH_PATH0 + p].on && pathLen[p]) drawChannel(CH_PATH0 + p, dtMs);
  }
  for (int z = 0; z < MAX_ZONES; z++) {
    if (zones[z].used && !zoneHidden[z] && states[CH_ZONE0 + z].on && zoneLen[z]) drawChannel(CH_ZONE0 + z, dtMs);
  }

  // "Never light" zones: dark whatever else covers them (zone paths, overlapping zones).
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    for (int i = 0; i < ledCount[o]; i++) if (lightHidden[o][i]) frame[o][i] = CRGB(0, 0, 0);
  }

  // Find a light: replaces everything with just the chosen lights.
  if (locateActive) drawLocate();
}
