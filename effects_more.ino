// effects_more - the larger effect set (v0.15.0). Called from drawChannel() in effects.ino.
//
// Settings each effect uses (the page shows only these, with these labels):
//   Rainbow wave    speed (lights/s), size x5 = lights per rainbow, direction
//   Rainbow cycle   speed (600/speed seconds per full rainbow)
//   Color wipe      colors, speed (lights/s), direction; 1 color = wipe on, then off
//   Gradient        colors, speed (1 = still, else drift lights/s), direction
//   Stripes         colors, size = stripe width, speed (1 = still), direction
//   Alternating     colors, size = group size, speed (swap every 10000/speed ms)
//   Color cycle     colors, fadeSeconds per color (holds, then fades to the next)
//   Twinkle         colors, size = density (x3 %), speed, background
//   Bounce          color, size = tail, speed, background
//   Center out/in   colors (alternate per ripple), size = ripple width, speed,
//                   reverse = inward, background. Whole house: center = the controller.
//   Meteor          color, size = tail, speed, direction, background
//   Running lights  color, size = wave length /2, speed, direction, background
//   Sparkle         color 1 = base, color 2 (or white) = sparkles, size = density, speed
//   Fire / candle   color, speed = flicker speed
//   Lightning       colors (1 = most strikes, others now and then), speed = how often,
//                   size = how far traveling strikes spread, background (off = see-through)
//   Strobe          color, speed = flashes (speed/10 per second, max 10), background
//   Sway            colors, size = stripe width (also how far it swings), speed/10 lights/s, reverse
//
// Everything is calculated from settings and elapsed time; "random" looks come from
// hash32(), so the same settings always give the same pattern.

// ---------- Helpers ----------

// Mixes the bits of x: a repeatable stand-in for random numbers.
uint32_t hash32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dUL;
  x ^= x >> 15; x *= 0x846ca68bUL;
  x ^= x >> 16;
  return x;
}

// Color k of the channel's color list (wraps around).
CRGB palColor(const LightState& s, int k) {
  int n = max((int)s.colorCount, 1);
  k = ((k % n) + n) % n;
  return k == 0 ? s.color : s.extra[k - 1];
}

// c at `level` (0-255) of the channel's brightness. Lit channels never round to 0.
CRGB litColor(CRGB c, const LightState& s, uint8_t level) {
  uint8_t b = scale8(s.brightness, level);
  if (b == 0) return CRGB(0, 0, 0);
  c.nscale8_video(b);
  return c;
}

// The background (at brightness) if it's on, otherwise off.
CRGB bgOrOff(const LightState& s) {
  if (!s.bgOn) return CRGB(0, 0, 0);
  return litColor(s.bgColor, s, 255);
}

// Sets line position i, following the channel's direction setting.
void putLight(int ch, int i, CRGB c) {
  int len = lineLength(ch);
  lineLight(ch, states[ch].reverse ? (len - 1 - i) : i) = c;
}

// t = 0-65535 from the first color to the last (no wrap).
CRGB palBlendLinear(const LightState& s, uint16_t t) {
  int n = s.colorCount;
  if (n <= 1) return s.color;
  uint32_t x = (uint32_t)t * (n - 1);
  int idx = x >> 16;
  if (idx >= n - 1) return palColor(s, n - 1);
  return blend(palColor(s, idx), palColor(s, idx + 1), (x >> 8) & 0xFF);
}

// pos = 0-65535 through all colors and back to the first (seamless loop).
CRGB palBlendCyclic(const LightState& s, uint16_t pos) {
  int n = s.colorCount;
  if (n <= 1) return s.color;
  uint32_t x = (uint32_t)pos * n;
  int idx = x >> 16;
  return blend(palColor(s, idx), palColor(s, idx + 1), (x >> 8) & 0xFF);
}

// Color for band k of a banded effect: with one color, bands alternate color / background.
CRGB bandColor(const LightState& s, int k) {
  if (s.colorCount <= 1) return (k % 2 == 0) ? litColor(s.color, s, 255) : bgOrOff(s);
  return litColor(palColor(s, k), s, 255);
}

int bandCount(const LightState& s) {
  return s.colorCount <= 1 ? 2 : s.colorCount;
}

// Brightness of a comet-style light d/256 lights behind the head, tail of tailLights.
uint8_t tailLevel(int32_t d, int32_t tailLights) {
  int32_t tail256 = tailLights * 256;
  if (d >= 0 && d < 256) return 255;                      // head
  if (d >= 256 && d < tail256 + 256) {
    uint8_t t = 255 - (uint8_t)(((d - 256) * 255) / tail256);
    return scale8(t, t);                                  // fades with distance squared
  }
  return 0;
}

// ---------- Color effects ----------

void drawRainbowWave(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  uint32_t span = max((uint32_t)s.size * 5, (uint32_t)2);  // lights per full rainbow
  uint32_t pos = advanceMotion(ch, dtMs, span);
  for (int i = 0; i < len; i++) {
    uint32_t x = ((uint32_t)i * 256 + span * 256 - pos) % (span * 256);
    CRGB c;
    hsv2rgb_rainbow(CHSV((uint8_t)(x / span), 255, 255), c);
    putLight(ch, i, litColor(c, s, 255));
  }
}

void drawRainbowCycle(int ch) {
  const LightState& s = states[ch];
  uint32_t period = 600000UL / s.speed;          // ms per full rainbow
  uint8_t hue = (uint8_t)(((effectMs[ch] % period) * 256) / period);
  CRGB c;
  hsv2rgb_rainbow(CHSV(hue, 255, 255), c);
  fillLine(ch, litColor(c, s, 255));
}

void drawWipe(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  if (len == 0) return;
  int stages = bandCount(s);
  uint32_t pos = advanceMotion(ch, dtMs, (uint32_t)len * stages) >> 8;
  int stage = pos / len, filled = pos % len;
  CRGB now = bandColor(s, stage), before = bandColor(s, stage + stages - 1);
  for (int i = 0; i < len; i++) putLight(ch, i, i < filled ? now : before);
}

void drawGradient(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  if (len == 0) return;
  if (s.speed <= 1) {
    // still: first color at one end, last color at the other
    for (int i = 0; i < len; i++) {
      uint16_t t = len > 1 ? (uint32_t)i * 65535 / (len - 1) : 0;
      putLight(ch, i, litColor(palBlendLinear(s, t), s, 255));
    }
    return;
  }
  // drifting: colors loop round so the movement is seamless
  uint32_t pos = advanceMotion(ch, dtMs, len);
  for (int i = 0; i < len; i++) {
    uint32_t x = ((uint32_t)i * 256 + (uint32_t)len * 256 - pos) % ((uint32_t)len * 256);
    putLight(ch, i, litColor(palBlendCyclic(s, (uint16_t)(x * 256 / len)), s, 255));
  }
}

void drawStripes(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  uint32_t band = s.size, period = band * bandCount(s);
  uint32_t shift = s.speed > 1 ? advanceMotion(ch, dtMs, period) >> 8 : 0;
  for (int i = 0; i < len; i++) {
    uint32_t k = ((i + period - shift % period) % period) / band;
    putLight(ch, i, bandColor(s, k));
  }
}

void drawAlternate(int ch) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  uint32_t interval = 10000UL / s.speed;
  uint32_t beat = effectMs[ch] / interval;
  int nb = bandCount(s);
  for (int i = 0; i < len; i++) putLight(ch, i, bandColor(s, (i / s.size + beat) % nb));
}

void drawColorCycle(int ch) {
  const LightState& s = states[ch];
  int n = s.colorCount;
  if (n <= 1) { fillLine(ch, litColor(s.color, s, 255)); return; }
  uint32_t per = (uint32_t)s.fadeSeconds * 1000;
  uint32_t t = effectMs[ch] % (per * n);
  int k = t / per;
  uint8_t f = (uint8_t)(((t % per) * 256) / per);
  // hold each color for 70% of its time, then fade to the next
  CRGB c = f < 180 ? palColor(s, k) : blend(palColor(s, k), palColor(s, k + 1), (uint8_t)((f - 180) * 255 / 75));
  fillLine(ch, litColor(c, s, 255));
}

void drawTwinkle(int ch) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  CRGB base = bgOrOff(s);
  uint32_t density = min((uint32_t)s.size * 3, (uint32_t)90);   // % of lights twinkling
  uint32_t t = effectMs[ch] * s.speed / 10;                     // speed 10 = normal
  for (int i = 0; i < len; i++) {
    uint32_t h = hash32(i * 2654435761UL + 0x9E37);
    uint32_t period = 1200 + h % 2400;              // each light has its own rhythm
    uint32_t phase = t + h % period;
    uint32_t cycle = phase / period, within = phase % period;
    uint32_t pick = hash32(i ^ (cycle * 0x85EBCA6BUL));
    CRGB c = base;
    if (pick % 100 < density) {
      uint8_t lvl = sin8((uint8_t)(within * 256 / period + 192));  // 0 -> peak -> 0
      c = blend(base, litColor(palColor(s, pick >> 8), s, 255), lvl);
    }
    putLight(ch, i, c);
  }
}

// ---------- Motion effects ----------

void drawBounce(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  if (len < 2) { fillLine(ch, litColor(s.color, s, 255)); return; }
  int32_t half = (int32_t)(len - 1) * 256;
  int32_t p = advanceMotion(ch, dtMs, 2 * (len - 1));
  bool forward = p <= half;
  int32_t head = forward ? p : 2 * half - p;
  for (int i = 0; i < len; i++) {
    int32_t d = forward ? head - (int32_t)i * 256 : (int32_t)i * 256 - head;
    setLight(ch, i, tailLevel(d, s.size));
  }
}

void drawCenter(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  if (len == 0) return;
  // Whole house: the controller sits between B1 and A1; otherwise the middle of the line.
  int32_t center = (ch == CH_HOUSE) ? (int32_t)strandLen[1] * 256 - 128 : (int32_t)(len - 1) * 128;
  int32_t width = (int32_t)s.size * 256;
  int32_t spacing = max((int32_t)s.size * 3, (int32_t)6) * 256;
  int32_t p = advanceMotion(ch, dtMs, spacing / 256);
  CRGB base = bgOrOff(s);
  for (int i = 0; i < len; i++) {
    int32_t dist = abs((int32_t)i * 256 - center);
    int32_t x = s.reverse ? dist + p : dist - p;        // reverse = rings move inward
    int32_t k = ((x % spacing) + spacing) % spacing;
    int32_t ring = (x - k) / spacing;
    CRGB c = base;
    if (k < width) {
      uint8_t lvl = 255 - (uint8_t)(abs(2 * k - width) * 255 / width);   // peak mid-ring
      c = blend(base, litColor(palColor(s, ring), s, 255), lvl);
    }
    lineLight(ch, i) = c;    // direction setting means in/out here, not flipped order
  }
}

void drawMeteor(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  int32_t head = (int32_t)advanceMotion(ch, dtMs, len + s.size);
  uint32_t slot = effectMs[ch] / 50;          // trail sparkle changes every 50 ms
  for (int i = 0; i < len; i++) {
    int32_t d = head - (int32_t)i * 256;
    uint8_t level = tailLevel(d, s.size);
    if (level && d >= 256) {
      uint32_t f = hash32(i * 7919UL + slot * 104729UL);
      level = (f & 0xFF) < 60 ? 0 : scale8(level, 128 + ((f >> 8) & 127));  // broken trail
    }
    setLight(ch, i, level);
  }
}

void drawRunning(int ch, uint32_t dtMs) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  uint32_t wave = max((uint32_t)s.size * 2, (uint32_t)4);
  uint32_t pos = advanceMotion(ch, dtMs, wave);
  CRGB base = bgOrOff(s), lit = litColor(s.color, s, 255);
  for (int i = 0; i < len; i++) {
    uint32_t x = ((uint32_t)i * 256 + wave * 256 - pos) % (wave * 256);
    putLight(ch, i, blend(base, lit, sin8((uint8_t)(x / wave))));
  }
}

// ---------- Sparkle & special ----------

void drawSparkle(int ch) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  CRGB base = litColor(s.color, s, 255);
  CRGB spark = litColor(s.colorCount >= 2 ? s.extra[0] : CRGB(255, 255, 255), s, 255);
  const uint32_t slotMs = 40;
  uint32_t slot = effectMs[ch] / slotMs, within = effectMs[ch] % slotMs;
  uint32_t perMille = (uint32_t)s.size * s.speed / 8;   // chance per light per 40 ms
  uint8_t lvl = 255 - within * 255 / slotMs;
  for (int i = 0; i < len; i++) {
    uint32_t h = hash32(i * 2246822519UL ^ slot * 3266489917UL);
    putLight(ch, i, (h % 1000 < perMille) ? blend(base, spark, lvl) : base);
  }
}

void drawFire(int ch) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  uint32_t t = effectMs[ch] * s.speed;
  for (int i = 0; i < len; i++) {
    uint8_t n = inoise8(i * 60, t / 16);                 // slow glow
    uint8_t flick = inoise8(i * 13 + 1000, t / 4);      // quick flickers
    uint8_t level = 60 + scale8(n, 195);
    if (flick > 200) level = qsub8(level, (flick - 200) * 2);
    putLight(ch, i, litColor(s.color, s, level));
  }
}

// Lightning (v0.27.0). One strike per time slot (speed = how often), at a random moment:
// a faint flicker (the leader), the main flash, often 1-3 restrikes with irregular dark
// gaps, then a short afterglow. Half the strikes light the whole line at once (sheet
// lightning); the others start at a random spot and race outward along the line, size =
// how far (30 = the whole line). Each strike is color 1, or now and then another color
// from the list. The flash adds to what's there, so it lights up the background color.
// Background off = see-through: between flashes the layers underneath show, e.g. a
// candle flicker on Whole house under lightning on a path. (A strand has nothing
// underneath, so it's dark there.) Same settings + same time = same strikes, so lines
// that start together (a scene) strike together.

// Brightness (0-255) of strike h, t ms after it starts (0 = over). stroke = which flash.
uint8_t strikeLevel(uint32_t h, uint32_t t, int& stroke) {
  stroke = 0;
  uint32_t lead = 20 + (h & 3) * 20;                       // leader flicker, 20-80 ms
  if (t < lead) return ((t / 10 + (h >> 2)) & 1) ? 50 : 15;
  t -= lead;
  static const uint8_t STROKES[8] = { 1, 2, 2, 3, 3, 2, 4, 3 };   // mostly double/triple
  int n = STROKES[(h >> 8) & 7];
  uint32_t start = 0;
  for (int j = 0; j < n; j++) {
    uint32_t hj = hash32(h ^ (j * 0x68E31DA4UL + 1));
    uint32_t on    = 30 + (hj & 3) * 15;                   // full brightness, 30-75 ms
    uint32_t decay = 40 + ((hj >> 2) & 3) * 20;            // then dims, 40-100 ms
    uint32_t gap   = 40 + ((hj >> 4) & 7) * 20;            // dark before the next, 40-180 ms
    uint8_t  peak  = (j == 0) ? 255 : 170 + ((hj >> 8) & 63);   // restrikes a bit dimmer
    stroke = j;
    if (t < start + on) return peak;
    if (t < start + on + decay) return lerp8by8(peak, 30, (t - start - on) * 255 / decay);
    start += on + decay;
    if (j < n - 1) {
      if (t < start + gap) return ((t / 20 + hj) % 3 == 0) ? 35 : 10;   // flickering gap
      start += gap;
    }
  }
  const uint32_t GLOW = 250;                               // afterglow
  if (t < start + GLOW) return 30 - (t - start) * 30 / GLOW;
  return 0;
}

void drawLightning(int ch) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  bool seeThrough = !s.bgOn && ch >= NUM_OUTPUTS;
  CRGB base = bgOrOff(s);
  if (!seeThrough) fillLine(ch, base);

  uint32_t slotMs = 60000UL / max((int)s.speed, 1);      // one strike per slot
  uint32_t slot = effectMs[ch] / slotMs, tIn = effectMs[ch] % slotMs;
  uint32_t h = hash32(slot * 0x9E3779B1UL + 17);
  uint32_t strikeAt = h % max(slotMs / 2, (uint32_t)1);
  if (tIn < strikeAt || len == 0) return;
  uint32_t t = tIn - strikeAt;
  int stroke;
  uint8_t level = strikeLevel(h, t, stroke);
  if (!level) return;

  // color: color 1 most of the time, sometimes another from the list
  uint32_t hc = hash32(h + 99);
  int k = (s.colorCount <= 1 || hc % 100 < 60) ? 0 : 1 + (int)(hc % (s.colorCount - 1));
  CRGB col = palColor(s, k);

  // where: the whole line, or spreading from a random spot (about 0.8 lights per ms)
  bool sheet = (h >> 12) & 1;
  int origin = (h >> 16) % len;
  int reach = sheet ? len : max(1, len * (int)s.size / SIZE_MAX);
  int front = sheet ? len : (int)min((uint32_t)reach, t * 4 / 5 + 1);
  for (int i = 0; i < len; i++) {
    int d = abs(i - origin);
    if (d > front) continue;
    uint8_t lv = level;
    if (!sheet) lv = scale8(lv, 255 - d * 128 / (reach + 1));             // dimmer toward the ends
    lv = scale8(lv, 200 + (hash32(h + i * 31 + stroke * 7919) & 55));    // a bit ragged
    lineLight(ch, i) += litColor(col, s, lv);
  }
}

void drawStrobe(int ch) {
  const LightState& s = states[ch];
  uint32_t period = 10000UL / s.speed;                  // speed 100 = 10 flashes per second
  uint32_t onMs = min(period / 2, (uint32_t)50);
  bool on = (effectMs[ch] % period) < onMs;
  fillLine(ch, on ? litColor(s.color, s, 255) : bgOrOff(s));
}

// Sway: stripes of `size` lights in each color glide one stripe width along the line,
// then back, and so on, blending smoothly between lights (slow at each end).
// speed/10 = lights per second (0.1-10); reverse = the first move goes the other way.
void drawSway(int ch) {
  const LightState& s = states[ch];
  int len = lineLength(ch);
  int band = max((int)s.size, 1), nb = bandCount(s);
  uint32_t halfMs = (uint32_t)band * 10000UL / max((int)s.speed, 1);  // one swing; speed/10 lights/s
  if (halfMs < 50) halfMs = 50;
  uint32_t t = effectMs[ch] % (2 * halfMs);
  uint32_t p = (uint32_t)(((uint64_t)(t < halfMs ? t : 2 * halfMs - t) << 16) / halfMs);   // 0-65536 and back
  int32_t off = (int32_t)(((uint64_t)band * 256 * easeInOut16(min(p, (uint32_t)65535))) >> 16);   // 1/256 lights
  int32_t period = band * nb;
  for (int i = 0; i < len; i++) {
    int32_t q = i * 256 - off;                                   // where this light's color comes from
    int32_t j = (q >= 0) ? (q >> 8) : -(((-q) + 255) >> 8);      // floor
    uint8_t frac = q - j * 256;
    int32_t k1 = ((j % period) + period) % period, k2 = (k1 + 1) % period;
    CRGB c = bandColor(s, k1 / band);
    if (frac) c = blend(c, bandColor(s, k2 / band), frac);
    putLight(ch, i, c);
  }
}
