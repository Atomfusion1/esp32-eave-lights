// animations - custom animations made of steps (MODE_ANIM shows animation number `size`).
//
// An animation works on a "frame": the channel's colors as bands of `bandWidth` lights,
// or one of the 8 paintings. Its steps play in order and then loop:
//   hold  N s            keep still
//   move  +/-L over N s  slide the frame L lights along the line (- = back); lights that
//                        leave one end come back in at the other. Smooth glide: colors
//                        blend between neighboring lights. Eased (slow start and stop)
//                        unless the step is "steady".
//   fade  to F over N s  cross-fade to frame F
//   cut   to F, hold N s switch to frame F at once
// Each loop starts again from the start frame; moves add up, so a loop that moves +3
// and -1 drifts 2 lights further every time round.
// Brightness, direction (reverse) and background (shown for black / unpainted lights)
// come from the channel's settings, like any other effect.
// Everything is calculated from the channel's elapsed time, so it is repeatable.
//
//   GET  /api/anims   {"anims":[{"id":1,"name","start":0,"band":3,"steps":"m3:2000,m-3:2000"}...]}
//   POST /api/anims   action=save id=N name=Dance start=0 band=3 steps=m3:2000,m-3:2000
//                     action=reset id=N   back to the default animation
// Steps text, comma separated, times in milliseconds:
//   h:MS  hold    m<L>:MS  move (add "s" at the end for steady: m5:3000s)
//   f<F>:MS  fade to frame F    c<F>:MS  cut to frame F      (F: 0 = colors, 1-8 painting)

const char*    ANIMS_FILE    = "/anims.bin";
const uint32_t ANIMS_MAGIC   = 0x4D494E41;  // "ANIM"
const uint16_t ANIMS_VERSION = 1;

// Default: the colors in 3-light bands, dancing 3 lights forward and back over 2 s each.
void defaultAnim(int n) {
  AnimDef& a = anims[n];
  memset(&a, 0, sizeof(a));
  snprintf(a.name, NAME_LEN, "Animation %d", n + 1);
  a.startFrame = 0;
  a.bandWidth = 3;
  a.stepCount = 2;
  a.steps[0] = { ANIM_MOVE, 3, 0, 0, 2000 };
  a.steps[1] = { ANIM_MOVE, -3, 0, 0, 2000 };
}

void sanitizeAnim(AnimDef& a) {
  a.name[NAME_LEN - 1] = 0;
  if (a.startFrame > MAX_PATTERNS) a.startFrame = 0;
  a.bandWidth = constrain((int)a.bandWidth, 1, SIZE_MAX);
  a.stepCount = min((int)a.stepCount, MAX_ANIM_STEPS);
  for (int k = 0; k < a.stepCount; k++) {
    AnimStep& s = a.steps[k];
    if (s.kind > ANIM_CUT) s.kind = ANIM_HOLD;
    if (s.frame > MAX_PATTERNS) s.frame = 0;
    s.lights = constrain((int)s.lights, -ANIM_MAX_MOVE, ANIM_MAX_MOVE);
    s.ms = min((uint32_t)s.ms, (uint32_t)ANIM_MAX_MS);
  }
}

void loadAnims() {
  for (int n = 0; n < MAX_ANIMS; n++) defaultAnim(n);
  if (!storageOk || !FFat.exists(ANIMS_FILE)) return;
  File f = FFat.open(ANIMS_FILE, "r");
  if (!f) return;
  FileHeader h;
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h) || h.magic != ANIMS_MAGIC ||
      h.version != ANIMS_VERSION || h.recordSize != sizeof(AnimDef)) {
    Serial.println("Animations: file is from a different firmware layout, using defaults");
    f.close();
    return;
  }
  for (int n = 0; n < h.count && n < MAX_ANIMS; n++) {
    AnimDef a;
    if (f.read((uint8_t*)&a, sizeof(a)) != sizeof(a)) break;
    sanitizeAnim(a);
    anims[n] = a;
  }
  f.close();
  Serial.println("Animations: loaded");
}

void saveAnims() {
  if (!storageOk || savesBlocked) return;
  File f = FFat.open(ANIMS_FILE, "w");
  if (!f) {
    Serial.println("Animations: could not write file");
    return;
  }
  FileHeader h = { ANIMS_MAGIC, ANIMS_VERSION, (uint16_t)sizeof(AnimDef), MAX_ANIMS };
  f.write((const uint8_t*)&h, sizeof(h));
  f.write((const uint8_t*)anims, sizeof(anims));
  f.close();
}

// ---------- Steps as text ----------

String animStepsText(const AnimDef& a) {
  String t;
  for (int k = 0; k < a.stepCount; k++) {
    const AnimStep& s = a.steps[k];
    if (k) t += ",";
    switch (s.kind) {
      case ANIM_MOVE: t += "m"; t += (int)s.lights; break;
      case ANIM_FADE: t += "f"; t += (int)s.frame; break;
      case ANIM_CUT:  t += "c"; t += (int)s.frame; break;
      default:        t += "h"; break;
    }
    t += ":";
    t += (unsigned)s.ms;
    if (s.kind == ANIM_MOVE && s.steady) t += "s";
  }
  return t;
}

// Parses steps text into a. Returns an error message, or nullptr if it is fine.
const char* parseAnimSteps(const String& text, AnimDef& a) {
  a.stepCount = 0;
  int pos = 0, len = text.length();
  while (pos < len) {
    int comma = text.indexOf(',', pos);
    if (comma < 0) comma = len;
    String item = text.substring(pos, comma);
    item.trim();
    pos = comma + 1;
    if (!item.length()) continue;
    if (a.stepCount >= MAX_ANIM_STEPS) return "up to 16 steps";
    int colon = item.indexOf(':');
    if (colon < 1) return "each step needs a time";
    AnimStep s = { ANIM_HOLD, 0, 0, 0, 0 };
    char kind = tolower(item[0]);
    int num = item.substring(1, colon).toInt();
    String msText = item.substring(colon + 1);
    if (msText.endsWith("s")) { s.steady = 1; msText.remove(msText.length() - 1); }
    long ms = msText.toInt();
    if (ms < 0 || ms > ANIM_MAX_MS) return "a step can take up to 60 seconds";
    s.ms = ms;
    if (kind == 'h') {
      s.kind = ANIM_HOLD;
    } else if (kind == 'm') {
      if (num < -ANIM_MAX_MOVE || num > ANIM_MAX_MOVE) return "a move can be up to 60 lights";
      s.kind = ANIM_MOVE;
      s.lights = num;
    } else if (kind == 'f' || kind == 'c') {
      if (num < 0 || num > MAX_PATTERNS) return "frame must be colors or painting 1 to 8";
      s.kind = kind == 'f' ? ANIM_FADE : ANIM_CUT;
      s.frame = num;
    } else {
      return "unknown step";
    }
    a.steps[a.stepCount++] = s;
  }
  return nullptr;
}

String animsJson() {
  String j = "{\"anims\":[";
  for (int n = 0; n < MAX_ANIMS; n++) {
    const AnimDef& a = anims[n];
    if (n) j += ",";
    j += "{\"id\":";
    j += n + 1;
    j += ",\"name\":\"";
    j += a.name;
    j += "\",\"start\":";
    j += a.startFrame;
    j += ",\"band\":";
    j += a.bandWidth;
    j += ",\"steps\":\"";
    j += animStepsText(a);
    j += "\"}";
  }
  j += "]}";
  return j;
}

void handleAnims() {
  String action = server.arg("action");
  if (action.length()) {
    int n = server.arg("id").toInt();
    if (n < 1 || n > MAX_ANIMS) return sendError("animation must be 1 to 8");
    n--;
    if (action == "save") {
      AnimDef a = anims[n];
      if (server.hasArg("name")) {
        char clean[NAME_LEN];
        if (!copyName(server.arg("name"), clean)) return sendError("enter a name");
        strcpy(a.name, clean);
      }
      if (server.hasArg("start")) a.startFrame = constrain(server.arg("start").toInt(), 0, MAX_PATTERNS);
      if (server.hasArg("band"))  a.bandWidth  = constrain(server.arg("band").toInt(), 1, SIZE_MAX);
      if (server.hasArg("steps")) {
        const char* err = parseAnimSteps(server.arg("steps"), a);
        if (err) return sendError(err);
      }
      sanitizeAnim(a);
      anims[n] = a;
    } else if (action == "reset") {
      defaultAnim(n);
    } else {
      return sendError("action must be save or reset");
    }
    saveAnims();
  }
  server.send(200, "application/json", animsJson());
}

// ---------- Drawing ----------

// 0-65535 -> 0-65535 with a slow start and stop (smoothstep).
uint32_t easeInOut16(uint32_t p) {
  return (uint32_t)(((uint64_t)p * p * (3 * 65536ULL - 2 * p)) >> 32);
}

// Raw color of frame f at line position j (black = nothing there, shows the background).
CRGB animFrameColor(int ch, const AnimDef& a, uint8_t f, int j) {
  const LightState& s = states[ch];
  CRGB c;
  if (f == 0) {
    c = palColor(s, j / max((int)a.bandWidth, 1));
  } else {
    int o, idx;
    if (!linePoint(ch, j, o, idx)) c = CRGB(0, 0, 0);   // path gap
    else c = patterns[f - 1][o][idx];
  }
  if (!(c.r | c.g | c.b) && s.bgOn) c = s.bgColor;
  return c;
}

void drawAnim(int ch) {
  const LightState& s = states[ch];
  const AnimDef& a = anims[constrain((int)s.size, 1, MAX_ANIMS) - 1];
  int len = lineLength(ch);
  if (len <= 0) return;

  // Where in the loop are we?
  uint32_t loopMs = 0;
  int32_t  loopMove = 0;
  for (int k = 0; k < a.stepCount; k++) {
    loopMs += a.steps[k].ms;
    if (a.steps[k].kind == ANIM_MOVE) loopMove += a.steps[k].lights;
  }
  uint8_t fromFrame = a.startFrame, toFrame = a.startFrame, mix = 0;
  int64_t offset = 0;   // in 1/256 lights
  if (loopMs > 0) {
    uint32_t t = effectMs[ch];
    uint32_t loops = t / loopMs;
    uint32_t tc = t % loopMs;
    offset = (int64_t)loops * loopMove * 256;
    for (int k = 0; k < a.stepCount; k++) {
      const AnimStep& st = a.steps[k];
      bool done = tc >= st.ms;
      uint32_t p = done ? 65536 : (uint32_t)(((uint64_t)tc << 16) / st.ms);   // 0-65536
      if (st.kind == ANIM_MOVE) {
        uint32_t e = (done || st.steady) ? p : easeInOut16(p);
        offset += ((int64_t)st.lights * 256 * e) >> 16;
      } else if (st.kind == ANIM_CUT) {
        fromFrame = toFrame = st.frame;
      } else if (st.kind == ANIM_FADE) {
        if (done) {
          fromFrame = toFrame = st.frame;
        } else {
          fromFrame = toFrame;
          toFrame = st.frame;
          mix = p >> 8;
        }
      }
      if (!done) break;
      tc -= st.ms;
    }
  }
  int64_t span = (int64_t)len * 256;
  int32_t off = (int32_t)(((offset % span) + span) % span);

  for (int i = 0; i < len; i++) {
    // the frame light that has moved to position i (wrapping around the line)
    int32_t p = i * 256 - off;
    if (p < 0) p += span;
    int j = p >> 8, j2 = (j + 1) % len;
    uint8_t frac = p & 0xFF;
    CRGB c1 = animFrameColor(ch, a, fromFrame, j);
    CRGB c2 = animFrameColor(ch, a, fromFrame, j2);
    if (mix) {
      c1 = blend(c1, animFrameColor(ch, a, toFrame, j), mix);
      c2 = blend(c2, animFrameColor(ch, a, toFrame, j2), mix);
    }
    CRGB c = frac ? blend(c1, c2, frac) : c1;
    putLight(ch, i, (c.r | c.g | c.b) ? litColor(c, s, 255) : CRGB(0, 0, 0));
  }
}
