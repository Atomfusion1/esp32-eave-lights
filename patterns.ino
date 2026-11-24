// patterns - custom painted patterns: a color for each physical light.
//
// MODE_CUSTOM shows pattern number `size` (1-MAX_PATTERNS) on the channel's lights,
// scaled by its brightness; black lights show the background (or stay off).
// A pattern belongs to the physical lights, so A12 has the same color whether it is
// shown through strand A, a zone, a path or Whole house.
//
//   GET  /api/custom?pattern=N   binary, same layout as /api/frame:
//                                [outputs], per output [count lo][count hi], RGB per light
//   POST /api/custom  pattern=N  lights=A12,A13,B4-9  color=RRGGBB   paint those lights
//                     pattern=N  action=fill color=RRGGBB            every light
//                     pattern=N  action=clear                        every light off
// Paint is saved to /patterns.bin PATTERN_SAVE_MS after the last change.

const char*    PATTERNS_FILE    = "/patterns.bin";
const uint32_t PATTERNS_MAGIC   = 0x4E545450;  // "PTTN"
const uint16_t PATTERNS_VERSION = 1;
const uint32_t PATTERN_SAVE_MS  = 2000;

bool     patternsDirty   = false;
uint32_t patternsDirtyMs = 0;

void drawCustom(int ch) {
  const LightState& s = states[ch];
  int p = constrain((int)s.size, 1, MAX_PATTERNS) - 1;
  int len = lineLength(ch);
  CRGB base = bgOrOff(s);
  for (int i = 0; i < len; i++) {
    int o, idx;
    if (!linePoint(ch, i, o, idx)) continue;           // path gap
    CRGB c = patterns[p][o][idx];
    lineLight(ch, i) = (c.r | c.g | c.b) ? litColor(c, s, 255) : base;
  }
}

// Paints lights listed like "A12,A13,B4-9" (light numbers 1-based). Returns lights painted.
int paintLights(int p, const String& list, CRGB color) {
  int painted = 0, pos = 0, len = list.length();
  while (pos < len) {
    while (pos < len && (list[pos] == ',' || list[pos] == ' ')) pos++;
    if (pos >= len) break;
    char strand = toupper(list[pos++]);
    int o = -1;
    for (int k = 0; k < NUM_OUTPUTS; k++) if (OUTPUT_LETTERS[k] == strand) o = k;
    int a = 0, b = 0;
    while (pos < len && isdigit((unsigned char)list[pos])) a = a * 10 + (list[pos++] - '0');
    b = a;
    if (pos < len && list[pos] == '-') {
      pos++;
      b = 0;
      while (pos < len && isdigit((unsigned char)list[pos])) b = b * 10 + (list[pos++] - '0');
    }
    while (pos < len && list[pos] != ',') pos++;      // skip anything unexpected
    if (o < 0 || a < 1 || b < 1) continue;
    if (a > b) { int t = a; a = b; b = t; }
    for (int i = a; i <= b && i <= ledCount[o]; i++) {
      patterns[p][o][i - 1] = color;
      painted++;
    }
  }
  return painted;
}

void handleGetCustom() {
  int p = constrain(server.arg("pattern").toInt(), 1, MAX_PATTERNS) - 1;
  static uint8_t buf[1 + NUM_OUTPUTS * 2 + MAX_TOTAL_LEDS * 3];
  size_t n = 0;
  buf[n++] = NUM_OUTPUTS;
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    buf[n++] = ledCount[o] & 0xFF;
    buf[n++] = ledCount[o] >> 8;
  }
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    for (int i = 0; i < ledCount[o]; i++) {
      buf[n++] = patterns[p][o][i].r;
      buf[n++] = patterns[p][o][i].g;
      buf[n++] = patterns[p][o][i].b;
    }
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (const char*)buf, n);
}

void handlePostCustom() {
  int p = server.arg("pattern").toInt();
  if (p < 1 || p > MAX_PATTERNS) return sendError("pattern must be 1 to 8");
  p--;
  String action = server.arg("action");
  CRGB color(0, 0, 0);
  if (server.hasArg("color") && !parseColorArg("color", color)) return sendError("color must be RRGGBB");
  int painted = 0;
  if (action == "fill" || action == "clear") {
    if (action == "clear") color = CRGB(0, 0, 0);
    for (int o = 0; o < NUM_OUTPUTS; o++) {
      for (int i = 0; i < MAX_LEDS; i++) patterns[p][o][i] = color;
      painted += ledCount[o];
    }
  } else {
    painted = paintLights(p, server.arg("lights"), color);
  }
  patternsDirty = true;
  patternsDirtyMs = millis();
  server.send(200, "application/json", String("{\"ok\":true,\"painted\":") + painted + "}");
}

void handlePatternSave() {
  if (!patternsDirty || millis() - patternsDirtyMs < PATTERN_SAVE_MS) return;
  if (savesBlocked) return;
  patternsDirty = false;
  savePatternsFile();
}

void loadPatterns() {
  memset(patterns, 0, sizeof(patterns));
  if (!storageOk || !FFat.exists(PATTERNS_FILE)) return;
  File f = FFat.open(PATTERNS_FILE, "r");
  if (!f) return;
  FileHeader h;
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h) || h.magic != PATTERNS_MAGIC ||
      h.version != PATTERNS_VERSION || h.recordSize != sizeof(patterns[0])) {
    Serial.println("Patterns: file is from a different firmware layout, starting empty");
    f.close();
    return;
  }
  for (int p = 0; p < h.count && p < MAX_PATTERNS; p++) {
    if (f.read((uint8_t*)patterns[p], sizeof(patterns[0])) != sizeof(patterns[0])) break;
  }
  f.close();
  Serial.println("Patterns: loaded");
}

void savePatternsFile() {
  if (!storageOk || savesBlocked) return;
  File f = FFat.open(PATTERNS_FILE, "w");
  if (!f) {
    Serial.println("Patterns: could not write file");
    return;
  }
  FileHeader h = { PATTERNS_MAGIC, PATTERNS_VERSION, (uint16_t)sizeof(patterns[0]), MAX_PATTERNS };
  f.write((const uint8_t*)&h, sizeof(h));
  f.write((const uint8_t*)patterns, sizeof(patterns));
  f.close();
}
