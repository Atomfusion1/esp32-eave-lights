// patterns - custom paintings: a color for each physical light.
//
// A library of up to MAX_PATTERNS named paintings. MODE_CUSTOM shows painting number
// `size` (1-MAX_PATTERNS) on the channel's lights, scaled by its brightness; black lights
// show the background (or stay off). A painting belongs to the physical lights, so A12
// has the same color whether it is shown through strand A, a zone, a path or Whole house.
//
// Paintings used by a saved scene or saved look are "locked" (see lookUsers() in
// scenes.ino): the page paints a copy instead, so saved scenes never change. A painting
// that is used anywhere can't be deleted.
//
//   GET  /api/custom?pattern=N   binary, same layout as /api/frame:
//                                [outputs], per output [count lo][count hi], RGB per light
//   POST /api/custom  pattern=N  lights=A12,A13,B4-9  color=RRGGBB   paint those lights
//                     pattern=N  action=fill color=RRGGBB            every light
//                     pattern=N  action=clear                        every light off
//                     add draft=1 to paint into the draft instead (see below)
//                     action=commit   copy the draft into its painting (Apply)
//                     action=discard  throw the draft away (Discard)
//   GET  /api/custom?pattern=N&draft=1   the draft, if painting N has one
// Draft: with the page in "on Apply" mode, paint goes to a copy of the painting
// (paintDraft, one painting at a time). Only the preview shows it, so the real lights
// keep the saved painting until Apply.
// Paint is saved to /patterns.bin PATTERN_SAVE_MS after the last change; only the
// paintings that changed are rewritten.

const char*    PATTERNS_FILE    = "/patterns.bin";
const uint32_t PATTERNS_MAGIC   = 0x4E545450;  // "PTTN"
const uint16_t PATTERNS_VERSION = 2;           // record = one painting; count grew 8 -> 30
                                               // v1 (before 0.36.0): 2 strands x 300 lights
const uint32_t PATTERN_SAVE_MS  = 2000;

// Painting names, which slots exist, and saved brush colors (swatches), in /paintmeta.bin.
//   GET  /api/paint-meta   {"paintings":[{"id","name","used":["Scene: Xmas",...],"locked"}],
//                           "swatches":["#RRGGBB",...]}
//   POST /api/paint-meta action=new name=Flag            -> also "newId"
//                        action=copy pattern=N [name=]   -> also "newId"
//                        action=rename pattern=N name=Flag
//                        action=delete pattern=N         (refused while it is used)
//                        action=swatches list=RRGGBB,RRGGBB,...   (replaces the list)
#define MAX_SWATCHES 12
const char*    PAINTMETA_FILE    = "/paintmeta.bin";
const uint32_t PAINTMETA_MAGIC   = 0x4154454D;  // "META"
const uint16_t PAINTMETA_VERSION = 2;

struct PaintMeta {
  char    names[MAX_PATTERNS][NAME_LEN];
  uint8_t exists[MAX_PATTERNS];      // 1 = this painting slot is in use
  uint8_t swatchCount;
  CRGB    swatches[MAX_SWATCHES];
};
// v1 (v0.18-v0.22): always 8 paintings, no exists[].
struct PaintMetaV1 {
  char    names[8][NAME_LEN];
  uint8_t swatchCount;
  CRGB    swatches[MAX_SWATCHES];
};
PaintMeta paintMeta;
bool      paintMetaLoaded = false;   // false = no file yet (exists[] guessed from the paint)

bool     patternsDirty     = false;
uint32_t patternsDirtyMs   = 0;
uint32_t patternsDirtyMask = 0;      // which paintings changed since the last save

bool paintingExists(int p) {
  return p >= 0 && p < MAX_PATTERNS && paintMeta.exists[p];
}

void markPatternDirty(int p) {
  patternsDirty = true;
  patternsDirtyMs = millis();
  patternsDirtyMask |= 1UL << p;
}

void defaultPaintMeta() {
  memset(&paintMeta, 0, sizeof(paintMeta));
  for (int p = 0; p < MAX_PATTERNS; p++) snprintf(paintMeta.names[p], NAME_LEN, "Painting %d", p + 1);
  paintMeta.exists[0] = 1;
  const CRGB defaults[] = { CRGB(255, 0, 0), CRGB(0, 255, 16), CRGB(255, 255, 255), CRGB(255, 140, 60),
                            CRGB(0, 48, 255), CRGB(255, 74, 0), CRGB(128, 0, 255), CRGB(255, 154, 0) };
  paintMeta.swatchCount = sizeof(defaults) / sizeof(defaults[0]);
  for (int k = 0; k < paintMeta.swatchCount; k++) paintMeta.swatches[k] = defaults[k];
}

void loadPaintMeta() {
  defaultPaintMeta();
  paintMetaLoaded = false;
  if (!storageOk || !FFat.exists(PAINTMETA_FILE)) return;
  File f = FFat.open(PAINTMETA_FILE, "r");
  if (!f) return;
  FileHeader h;
  if (f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == PAINTMETA_MAGIC) {
    if (h.version == PAINTMETA_VERSION && h.recordSize == sizeof(PaintMeta)) {
      PaintMeta m;
      if (f.read((uint8_t*)&m, sizeof(m)) == sizeof(m)) { paintMeta = m; paintMetaLoaded = true; }
    } else if (h.version == 1 && h.recordSize == sizeof(PaintMetaV1)) {
      // converter: the old 8 paintings keep their names and all stay
      PaintMetaV1 m;
      if (f.read((uint8_t*)&m, sizeof(m)) == sizeof(m)) {
        for (int p = 0; p < 8; p++) { memcpy(paintMeta.names[p], m.names[p], NAME_LEN); paintMeta.exists[p] = 1; }
        paintMeta.swatchCount = m.swatchCount;
        memcpy(paintMeta.swatches, m.swatches, sizeof(m.swatches));
        paintMetaLoaded = true;
        Serial.println("Paintings: converted names from v1");
      }
    }
  }
  f.close();
  for (int p = 0; p < MAX_PATTERNS; p++) paintMeta.names[p][NAME_LEN - 1] = 0;
  paintMeta.swatchCount = min((int)paintMeta.swatchCount, MAX_SWATCHES);
}

void savePaintMeta() {
  if (!storageOk || savesBlocked) return;
  File f = FFat.open(PAINTMETA_FILE, "w");
  if (!f) return;
  FileHeader h = { PAINTMETA_MAGIC, PAINTMETA_VERSION, (uint16_t)sizeof(PaintMeta), 1 };
  f.write((const uint8_t*)&h, sizeof(h));
  f.write((const uint8_t*)&paintMeta, sizeof(paintMeta));
  f.close();
}

String paintMetaJson() {
  String j = "{\"paintings\":[";
  bool first = true;
  for (int p = 0; p < MAX_PATTERNS; p++) {
    if (!paintMeta.exists[p]) continue;
    bool locked = false;
    String users = lookUsers(0, p, locked, false);
    if (!first) j += ",";
    first = false;
    j += "{\"id\":" + String(p + 1) + ",\"name\":\"" + paintMeta.names[p] + "\",\"used\":" + users +
         ",\"locked\":" + (locked ? "true" : "false") + "}";
  }
  j += "],\"swatches\":[";
  for (int k = 0; k < paintMeta.swatchCount; k++) {
    char c[12];
    snprintf(c, sizeof(c), "%s\"#%02X%02X%02X\"", k ? "," : "",
             paintMeta.swatches[k].r, paintMeta.swatches[k].g, paintMeta.swatches[k].b);
    j += c;
  }
  j += "]";
  return j;   // caller adds "}" (and maybe "newId")
}

// A free painting slot, or -1.
int freePainting() {
  for (int p = 0; p < MAX_PATTERNS; p++) if (!paintMeta.exists[p]) return p;
  return -1;
}

// "<name> copy" (cut to fit).
void copyNameOf(const char* name, char* out) {
  char tmp[NAME_LEN + 8];
  snprintf(tmp, sizeof(tmp), "%s copy", name);
  if (strlen(tmp) >= NAME_LEN) {   // keep " copy" visible
    int keep = NAME_LEN - 1 - 5;
    memcpy(tmp, name, keep);
    strcpy(tmp + keep, " copy");
  }
  strncpy(out, tmp, NAME_LEN - 1);
  out[NAME_LEN - 1] = 0;
}

void handlePaintMeta() {
  String action = server.arg("action");
  int newId = -1;
  int p = server.arg("pattern").toInt() - 1;
  if (action == "new" || action == "copy") {
    if (action == "copy" && !paintingExists(p)) return sendError("painting not found");
    int n = freePainting();
    if (n < 0) return sendError("all 30 paintings are used; delete one first");
    char clean[NAME_LEN];
    if (server.hasArg("name") && copyName(server.arg("name"), clean)) {
      strcpy(paintMeta.names[n], clean);
    } else if (action == "copy") {
      copyNameOf(paintMeta.names[p], paintMeta.names[n]);
    } else {
      snprintf(paintMeta.names[n], NAME_LEN, "Painting %d", n + 1);
    }
    if (action == "copy") {
      // a copy of what the lights would show after Apply: the draft if there is one
      memcpy(patterns[n], draftPattern == p ? paintDraft : patterns[p], sizeof(patterns[0]));
    } else {
      memset(patterns[n], 0, sizeof(patterns[0]));
    }
    paintMeta.exists[n] = 1;
    markPatternDirty(n);
    savePaintMeta();
    newId = n + 1;
  } else if (action == "rename") {
    if (!paintingExists(p)) return sendError("painting not found");
    char clean[NAME_LEN];
    if (!copyName(server.arg("name"), clean)) return sendError("enter a name");
    strcpy(paintMeta.names[p], clean);
    savePaintMeta();
  } else if (action == "delete") {
    if (!paintingExists(p)) return sendError("painting not found");
    bool locked = false;
    String users = lookUsers(0, p, locked, true);
    if (users != "[]") return sendError(("can't delete, still used by: " + listText(users)).c_str());
    paintMeta.exists[p] = 0;
    if (draftPattern == p) draftPattern = -1;
    memset(patterns[p], 0, sizeof(patterns[0]));
    markPatternDirty(p);
    savePaintMeta();
  } else if (action == "swatches") {
    String list = server.arg("list");
    uint8_t n = 0;
    int pos = 0;
    while (pos < (int)list.length() && n < MAX_SWATCHES) {
      int comma = list.indexOf(',', pos);
      if (comma < 0) comma = list.length();
      String hex = list.substring(pos, comma);
      hex.trim();
      if (hex.startsWith("#")) hex = hex.substring(1);
      pos = comma + 1;
      if (hex.length() != 6) continue;
      paintMeta.swatches[n++] = CRGB((uint32_t)strtoul(hex.c_str(), nullptr, 16));
    }
    paintMeta.swatchCount = n;
    savePaintMeta();
  } else if (action.length()) {
    return sendError("action must be new, copy, rename, delete or swatches");
  }
  String j = paintMetaJson();
  if (newId > 0) j += ",\"newId\":" + String(newId);
  j += "}";
  server.send(200, "application/json", j);
}

// Painting p's colors on output o: its draft while drawing the preview, else the saved one.
const CRGB* paintingRow(int p, int o) {
  return (previewPass && draftPattern == p) ? paintDraft[o] : patterns[p][o];
}

void drawCustom(int ch) {
  const LightState& s = states[ch];
  int p = constrain((int)s.size, 1, MAX_PATTERNS) - 1;
  int len = lineLength(ch);
  CRGB base = bgOrOff(s);
  bool have = paintingExists(p);
  for (int i = 0; i < len; i++) {
    int o, idx;
    if (!linePoint(ch, i, o, idx)) continue;           // path gap
    CRGB c = have ? paintingRow(p, o)[idx] : CRGB(0, 0, 0);
    lineLight(ch, i) = (c.r | c.g | c.b) ? litColor(c, s, 255) : base;
  }
}

// Paints lights listed like "A12,A13,B4-9" (light numbers 1-based). Returns lights painted.
int paintLights(int p, bool draft, const String& list, CRGB color) {
  CRGB (*dst)[MAX_LEDS] = draft ? paintDraft : patterns[p];
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
      dst[o][i - 1] = color;
      painted++;
    }
  }
  return painted;
}

void handleGetCustom() {
  int p = constrain(server.arg("pattern").toInt(), 1, MAX_PATTERNS) - 1;
  CRGB (*src)[MAX_LEDS] = (server.arg("draft") == "1" && draftPattern == p) ? paintDraft : patterns[p];
  uint8_t* buf = frameBytes;   // shared reply buffer (PSRAM)
  size_t n = 0;
  buf[n++] = NUM_OUTPUTS;
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    buf[n++] = ledCount[o] & 0xFF;
    buf[n++] = ledCount[o] >> 8;
  }
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    for (int i = 0; i < ledCount[o]; i++) {
      buf[n++] = src[o][i].r;
      buf[n++] = src[o][i].g;
      buf[n++] = src[o][i].b;
    }
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (const char*)buf, n);
}

void handlePostCustom() {
  String action = server.arg("action");
  // Apply / Discard of unapplied paint
  if (action == "commit" || action == "discard") {
    if (action == "commit" && draftPattern >= 0) {
      memcpy(patterns[draftPattern], paintDraft, sizeof(paintDraft));
      markPatternDirty(draftPattern);
    }
    draftPattern = -1;
    server.send(200, "application/json", "{\"ok\":true}");
    return;
  }

  int p = server.arg("pattern").toInt() - 1;
  if (!paintingExists(p)) return sendError("painting not found");
  // draft=1: paint into the draft (a copy of the painting) instead of the painting itself
  bool draft = server.arg("draft") == "1";
  if (draft && draftPattern != p) {
    memcpy(paintDraft, patterns[p], sizeof(paintDraft));
    draftPattern = p;
  }
  CRGB (*dst)[MAX_LEDS] = draft ? paintDraft : patterns[p];
  CRGB color(0, 0, 0);
  if (server.hasArg("color") && !parseColorArg("color", color)) return sendError("color must be RRGGBB");
  int painted = 0;
  if (action == "fill" || action == "clear") {
    if (action == "clear") color = CRGB(0, 0, 0);
    for (int o = 0; o < NUM_OUTPUTS; o++) {
      for (int i = 0; i < MAX_LEDS; i++) dst[o][i] = color;
      painted += ledCount[o];
    }
  } else {
    painted = paintLights(p, draft, server.arg("lights"), color);
  }
  if (!draft) markPatternDirty(p);
  server.send(200, "application/json", String("{\"ok\":true,\"painted\":") + painted + "}");
}

void handlePatternSave() {
  if (!patternsDirty || millis() - patternsDirtyMs < PATTERN_SAVE_MS) return;
  if (savesBlocked) return;
  patternsDirty = false;
  savePatternsFile(false);
}

void loadPatterns() {
  if (!patterns) {
    size_t bytes = (size_t)MAX_PATTERNS * sizeof(patterns[0]);
    patterns = (CRGB(*)[NUM_OUTPUTS][MAX_LEDS])ps_malloc(bytes);           // PSRAM
    if (!patterns) patterns = (CRGB(*)[NUM_OUTPUTS][MAX_LEDS])malloc(bytes);
  }
  memset(patterns, 0, (size_t)MAX_PATTERNS * sizeof(patterns[0]));
  draftPattern = -1;
  patternsDirtyMask = 0;
  loadPaintMeta();
  if (!storageOk || !FFat.exists(PATTERNS_FILE)) return;
  File f = FFat.open(PATTERNS_FILE, "r");
  if (!f) return;
  FileHeader h;
  const size_t oldRec = (size_t)LEGACY_OUTPUTS * LEGACY_MAX_LEDS * sizeof(CRGB);
  bool okHeader = f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == PATTERNS_MAGIC;
  bool isV1 = okHeader && h.version == 1 && h.recordSize == oldRec;
  bool isV2 = okHeader && h.version == PATTERNS_VERSION && h.recordSize == sizeof(patterns[0]);
  if (!isV1 && !isV2) {
    Serial.println("Paintings: file is from a different firmware layout, starting empty");
    f.close();
    return;
  }
  int loaded = 0;
  CRGB* old = isV1 ? (CRGB*)malloc(oldRec) : nullptr;   // one old painting (1.8 KB)
  for (int p = 0; p < h.count && p < MAX_PATTERNS; p++) {
    if (isV1) {
      // before v0.36.0: strands A and B of up to 300 lights; C-F stay unpainted
      if (!old || f.read((uint8_t*)old, oldRec) != oldRec) break;
      for (int o = 0; o < LEGACY_OUTPUTS; o++) memcpy(patterns[p][o], old + o * LEGACY_MAX_LEDS, LEGACY_MAX_LEDS * sizeof(CRGB));
    } else if (f.read((uint8_t*)patterns[p], sizeof(patterns[0])) != sizeof(patterns[0])) {
      break;
    }
    loaded++;
  }
  free(old);
  f.close();
  // Older firmware with no names file: every painting that has paint in it exists.
  if (!paintMetaLoaded) {
    for (int p = 0; p < loaded; p++) {
      for (int o = 0; o < NUM_OUTPUTS && !paintMeta.exists[p]; o++)
        for (int i = 0; i < MAX_LEDS; i++)
          if (patterns[p][o][i].r | patterns[p][o][i].g | patterns[p][o][i].b) { paintMeta.exists[p] = 1; break; }
    }
  }
  // An old file with fewer records or the old layout: rewrite it at full size, so
  // single paintings can be saved.
  if (h.count != MAX_PATTERNS || isV1) savePatternsFile(true);
  Serial.printf("Paintings: loaded %d%s\n", loaded, isV1 ? " (converted from the old format)" : "");
}

// Saves the paintings that changed (all = every painting, rewriting the file).
void savePatternsFile(bool all) {
  if (!storageOk || savesBlocked || !patterns) return;
  const size_t rec = sizeof(patterns[0]);
  if (!all && FFat.exists(PATTERNS_FILE)) {
    File f = FFat.open(PATTERNS_FILE, "r+");
    FileHeader h;
    if (f && f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == PATTERNS_MAGIC &&
        h.recordSize == rec && h.count == MAX_PATTERNS) {
      for (int p = 0; p < MAX_PATTERNS; p++) {
        if (!(patternsDirtyMask & (1UL << p))) continue;
        f.seek(sizeof(h) + p * rec);
        f.write((const uint8_t*)patterns[p], rec);
      }
      f.close();
      patternsDirtyMask = 0;
      return;
    }
    if (f) f.close();
  }
  File f = FFat.open(PATTERNS_FILE, "w");
  if (!f) {
    Serial.println("Paintings: could not write file");
    return;
  }
  FileHeader h = { PATTERNS_MAGIC, PATTERNS_VERSION, (uint16_t)rec, MAX_PATTERNS };
  f.write((const uint8_t*)&h, sizeof(h));
  f.write((const uint8_t*)patterns, MAX_PATTERNS * rec);
  f.close();
  patternsDirtyMask = 0;
}
