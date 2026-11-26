// led_output — strands A-F: their settings, start-up, power caps, sending frames.
//
// Each strand has a name, a length (ledCount[]), a type, a pin and a color order, set
// on the page (Setup > Strands, POST /api/strands) and saved in Preferences:
//   "strands"  = StrandCfg[NUM_OUTPUTS] (name, type, pin, color order)
//   "a_cnt", "b_cnt", "sc_cnt" .. "sf_cnt" = lengths (outputKey() in settings_store.ino)
// Types: off (not drawn, nothing sent), wired (FastLED on a GPIO pin), radio (drawn and
// shown in the live view; sent to a second board by ESP-NOW in a later version).
//
// FastLED registers a pin once, so a wired strand's pin (or switching a strand to wired)
// takes effect after a restart; the page offers a Restart button. Switching a strand off
// works at once (it is sent dark from then on).
//
// Strands are registered at MAX_LEDS and cleared at power-up, so lights past the saved
// length are sent OFF too; then applyLedCounts() trims each strand to its real length.
// Colors go frame[] (effects) -> out[] (power caps, RGB; the live view) -> wire[] (the
// strand's color order) -> FastLED, which is registered as RGB on every pin.

CLEDController* controllers[NUM_OUTPUTS] = { nullptr };

// GPIOs a strand may use. Left out: 0, 3, 45, 46 (boot strapping), 19-20 (USB),
// 26-37 (flash and octal PSRAM), 43-44 (Serial to the CH343 / COM13), 48 (RGB LED on
// many boards).
const uint8_t STRAND_PINS[] = { 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,
                                21, 38, 39, 40, 41, 42, 47 };
const char* ORDER_NAMES[] = { "RGB", "RBG", "GRB", "GBR", "BRG", "BGR" };
const char* TYPE_NAMES[]  = { "off", "wired", "radio" };

// The board's own RGB LED (a WS2812, GRB) can copy one light of a strand, e.g. a flood
// light, to test effects without the real lights. Saved in Preferences "boardled".
// GPIO 48 on most ESP32-S3 boards (38 on some newer DevKitC-1 v1.1).
const uint8_t BOARD_LED_PINS[] = { 48, 38 };
#define BOARD_LED_LEVEL 64   // 25 %: the LED is right on the board and very bright
struct BoardLedCfg {
  int8_t  strand;   // -1 = off, else the strand it copies
  uint8_t light;    // which light of that strand (1-based)
  uint8_t pin;
};
BoardLedCfg boardLed = { -1, 1, 48 };
uint8_t boardLedBootPin = 0;            // registered at start-up, 0 = none
CRGB    boardLedBuf[1];
CLEDController* boardLedCtl = nullptr;

bool boardLedPinAllowed(int pin) {
  for (uint8_t p : BOARD_LED_PINS) if (p == pin) return true;
  return false;
}

bool pinAllowed(int pin) {
  for (uint8_t p : STRAND_PINS) if (p == pin) return true;
  return false;
}

// FastLED needs the pin as a template argument, so each allowed pin gets its own case.
CLEDController* addStrandOnPin(uint8_t pin, CRGB* buf) {
  switch (pin) {
#define STRAND_PIN(P) case P: return &FastLED.addLeds<WS2811, P, RGB>(buf, MAX_LEDS);
    STRAND_PIN(1)  STRAND_PIN(2)  STRAND_PIN(4)  STRAND_PIN(5)  STRAND_PIN(6)  STRAND_PIN(7)
    STRAND_PIN(8)  STRAND_PIN(9)  STRAND_PIN(10) STRAND_PIN(11) STRAND_PIN(12) STRAND_PIN(13)
    STRAND_PIN(14) STRAND_PIN(15) STRAND_PIN(16) STRAND_PIN(17) STRAND_PIN(18) STRAND_PIN(21)
    STRAND_PIN(38) STRAND_PIN(39) STRAND_PIN(40) STRAND_PIN(41) STRAND_PIN(42) STRAND_PIN(47)
    STRAND_PIN(48)   // board LED only
#undef STRAND_PIN
  }
  return nullptr;
}

// ---- Settings ----

// First start (nothing saved): A and B wired to their pins as before, C-F off.
void defaultStrandCfg() {
  const uint8_t pins[NUM_OUTPUTS] = { PIN_OUTPUT_A, PIN_OUTPUT_B, 10, 11, 12, 13 };
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    StrandCfg& c = strandCfg[o];
    memset(&c, 0, sizeof(c));
    c.type  = o < HOUSE_STRANDS ? STRAND_WIRED : STRAND_OFF;
    c.pin   = pins[o];
    c.order = o < HOUSE_STRANDS ? ORDER_RBG : ORDER_RGB;   // the eaves are RBG
  }
  strcpy(strandCfg[0].name, "left");
  strcpy(strandCfg[1].name, "right");
}

void sanitizeStrandCfg(StrandCfg& c) {
  c.name[STRAND_NAME_LEN - 1] = 0;
  if (c.type > STRAND_RADIO) c.type = STRAND_OFF;
  if (c.order > ORDER_BGR) c.order = ORDER_RGB;
  if (!pinAllowed(c.pin)) { c.pin = 0; if (c.type == STRAND_WIRED) c.type = STRAND_OFF; }
}

void loadStrandCfg() {
  defaultStrandCfg();
  Preferences p;
  p.begin(PREFS_NAMESPACE, true);
  if (p.getBytesLength("strands") == sizeof(strandCfg)) p.getBytes("strands", strandCfg, sizeof(strandCfg));
  if (p.getBytesLength("boardled") == sizeof(boardLed)) p.getBytes("boardled", &boardLed, sizeof(boardLed));
  p.end();
  for (int o = 0; o < NUM_OUTPUTS; o++) sanitizeStrandCfg(strandCfg[o]);
  if (boardLed.strand < -1 || boardLed.strand >= NUM_OUTPUTS) boardLed.strand = -1;
  if (boardLed.light < 1) boardLed.light = 1;
  if (!boardLedPinAllowed(boardLed.pin)) boardLed.pin = BOARD_LED_PINS[0];
}

void saveStrandCfg() {
  if (restoreBlocksSaving()) return;
  Preferences p;
  p.begin(PREFS_NAMESPACE, false);
  p.putBytes("strands", strandCfg, sizeof(strandCfg));
  p.putBytes("boardled", &boardLed, sizeof(boardLed));
  p.end();
}

// The board LED's pin as a strand pin: a wired strand on it wins (no board LED then).
bool boardLedPinFree(uint8_t pin) {
  for (int o = 0; o < NUM_OUTPUTS; o++) if (bootPin[o] == pin) return false;
  return true;
}

// True if the strand is drawn (wired or radio). Off strands have no line (strandLen 0).
bool strandActive(int o) { return strandCfg[o].type != STRAND_OFF; }

// True if the saved settings need a restart to take effect (a wired pin not running yet).
bool restartNeeded() {
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    uint8_t want = strandCfg[o].type == STRAND_WIRED ? strandCfg[o].pin : 0;
    if (want && want != bootPin[o]) return true;
  }
  if (boardLed.strand >= 0 && boardLed.pin != boardLedBootPin) return true;
  return false;
}

// "Strand A (left)" for Serial messages.
String strandLabel(int o) {
  String s = String("Strand ") + OUTPUT_LETTERS[o];
  if (strandCfg[o].name[0]) s += String(" (") + strandCfg[o].name + ")";
  return s;
}

void printStrands() {
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    const StrandCfg& c = strandCfg[o];
    if (c.type == STRAND_OFF) continue;
    Serial.printf("%s: %s", strandLabel(o).c_str(), TYPE_NAMES[c.type]);
    if (c.type == STRAND_WIRED) {
      if (bootPin[o]) Serial.printf(" GPIO %u", bootPin[o]);
      else Serial.print(" (pin not running: restart)");
    }
    Serial.printf(", %u lights, %s\n", ledCount[o], ORDER_NAMES[c.order]);
  }
  if (boardLed.strand >= 0) {
    Serial.printf("Board LED (GPIO %u): copies %c%u%s\n", boardLed.pin, OUTPUT_LETTERS[boardLed.strand],
                  boardLed.light, boardLedBootPin ? "" : " (not running: restart)");
  }
}

// ---- Start-up ----

// Registers the wired strands with FastLED and sends OFF twice.
// Called first in setup() so the power-up white ends as soon as possible.
void initOutputs() {
  loadStrandCfg();
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    const StrandCfg& c = strandCfg[o];
    if (c.type != STRAND_WIRED) continue;
    bool taken = false;
    for (int k = 0; k < o; k++) if (bootPin[k] == c.pin) taken = true;
    if (taken) continue;   // two strands on one pin: only the first one runs
    controllers[o] = addStrandOnPin(c.pin, wire[o]);
    if (controllers[o]) bootPin[o] = c.pin;
  }
  if (boardLed.strand >= 0 && boardLedPinFree(boardLed.pin)) {
    boardLedCtl = addStrandOnPin(boardLed.pin, boardLedBuf);
    if (boardLedCtl) {
      boardLedCtl->setLeds(boardLedBuf, 1);
      boardLedBootPin = boardLed.pin;
    }
  }
  FastLED.setBrightness(255);  // brightness is limited by capPixel() instead
  FastLED.clear(true);
  delay(5);
  FastLED.clear(true);         // second OFF frame in case the first was missed
}

// Tells FastLED each strand's current length.
void applyLedCounts() {
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    if (controllers[o]) controllers[o]->setLeds(wire[o], ledCount[o]);
  }
}

// Sends every light on every pin dark, over the full MAX_LEDS (e.g. before a firmware update).
void sendAllDark() {
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    fill_solid(out[o], MAX_LEDS, CRGB(0, 0, 0));
    fill_solid(wire[o], MAX_LEDS, CRGB(0, 0, 0));
  }
  boardLedBuf[0] = CRGB(0, 0, 0);
  FastLED.show();
}

// Changes strand lengths (from the page). Sends OFF over the old lengths first, so
// lights past a new, shorter end don't keep their last color. Then rebuilds
// everything that depends on the lengths.
void setLedCounts(const uint16_t* counts) {
  sendAllDark();
  for (int o = 0; o < NUM_OUTPUTS; o++) ledCount[o] = constrain(counts[o], 1, MAX_LEDS);
  applyLedCounts();
  for (int o = 0; o < NUM_OUTPUTS; o++) fill_solid(frame[o], MAX_LEDS, CRGB(0, 0, 0));
  strandsChanged();
  Serial.print("Strand lengths:");
  for (int o = 0; o < NUM_OUTPUTS; o++) Serial.printf(" %c %u", OUTPUT_LETTERS[o], ledCount[o]);
  Serial.println();
}

// Rebuilds what depends on which strands are on and how long they are.
void strandsChanged() {
  initPaths();
  for (int z = 0; z < MAX_ZONES; z++) rebuildZone(z);
  rebuildAllZonePaths();
  markStateChanged();
}

// ---- Sending ----

// Limits one pixel to the measured safe levels.
// First scales the whole color down if R+G+B is over the sum cap (keeps hue),
// then clamps each channel to its own cap.
CRGB capPixel(CRGB c) {
  uint16_t sum = (uint16_t)c.r + c.g + c.b;
  if (sum > CAP_CHANNEL_SUM) {
    c.r = (uint32_t)c.r * CAP_CHANNEL_SUM / sum;
    c.g = (uint32_t)c.g * CAP_CHANNEL_SUM / sum;
    c.b = (uint32_t)c.b * CAP_CHANNEL_SUM / sum;
  }
  if (c.r > CAP_RED)   c.r = CAP_RED;
  if (c.g > CAP_GREEN) c.g = CAP_GREEN;
  if (c.b > CAP_BLUE)  c.b = CAP_BLUE;
  return c;
}

// An RGB color rearranged so that FastLED (registered as RGB) sends its bytes in the
// strand's order, e.g. RBG sends red, blue, green.
CRGB orderColor(CRGB c, uint8_t order) {
  switch (order) {
    case ORDER_RBG: return CRGB(c.r, c.b, c.g);
    case ORDER_GRB: return CRGB(c.g, c.r, c.b);
    case ORDER_GBR: return CRGB(c.g, c.b, c.r);
    case ORDER_BRG: return CRGB(c.b, c.r, c.g);
    case ORDER_BGR: return CRGB(c.b, c.g, c.r);
    default:        return c;
  }
}

// Applies caps to every pixel in frame[] and sends the result to all outputs.
// A strand switched off (or moved to another pin) since start-up is sent dark.
void showFrame() {
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    const StrandCfg& c = strandCfg[o];
    if (!strandActive(o)) {
      fill_solid(out[o], ledCount[o], CRGB(0, 0, 0));
    } else {
      for (int i = 0; i < ledCount[o]; i++) out[o][i] = capPixel(frame[o][i]);
    }
    if (!controllers[o]) continue;
    bool sending = c.type == STRAND_WIRED && c.pin == bootPin[o];
    for (int i = 0; i < ledCount[o]; i++) wire[o][i] = sending ? orderColor(out[o][i], c.order) : CRGB(0, 0, 0);
  }
  if (boardLedCtl) {
    CRGB c(0, 0, 0);
    int s = boardLed.strand, i = boardLed.light - 1;
    if (s >= 0 && i < ledCount[s]) c = out[s][i];
    c.nscale8_video(BOARD_LED_LEVEL);
    boardLedBuf[0] = orderColor(c, ORDER_GRB);   // WS2812: green, red, blue
  }
  FastLED.show();
}

// ---- Restart (to start a new pin) ----

uint32_t restartAtMs = 0;

void requestRestart() { restartAtMs = millis() + 700; }   // after the reply has gone out

void handleRestartRequest() {
  if (!restartAtMs || (int32_t)(millis() - restartAtMs) < 0) return;
  handleStateSaveNow();
  savePatternsFile(false);
  sendAllDark();
  Serial.println("Restarting for the new strand settings");
  delay(100);
  ESP.restart();
}

// ---- API: POST /api/strands ----
//   a=67 b=65 [c=.. f=..]          lengths only (older pages, backup restore)
//   strand=C [count=250] [name=Palm tree] [type=off|wired|radio] [pin=10] [order=RGB]
//   boardLed=D|off [boardLedLight=1] [boardLedPin=48]   the board's RGB LED copies D1
//   action=restart                 restart now (starts new pins)
// Returns the state ("outputs" has each strand's settings, "restartNeeded", "pins").
void handleStrands() {
  if (server.arg("action") == "restart") {
    requestRestart();
    server.send(200, "application/json", "{\"ok\":true,\"restarting\":true}");
    return;
  }
  uint16_t counts[NUM_OUTPUTS];
  for (int o = 0; o < NUM_OUTPUTS; o++) counts[o] = ledCount[o];
  bool countChanged = false;
  // lengths by letter: a=67 b=65 ...
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    char key[2] = { (char)tolower(OUTPUT_LETTERS[o]), 0 };
    if (!server.hasArg(key)) continue;
    int n = server.arg(key).toInt();
    if (n < 1 || n > MAX_LEDS) return sendError("each strand needs 1 to 400 lights");
    if (n != counts[o]) { counts[o] = n; countChanged = true; }
  }
  // one strand's settings
  if (server.hasArg("strand")) {
    String id = server.arg("strand");
    id.toUpperCase();
    int o = -1;
    for (int k = 0; k < NUM_OUTPUTS; k++) if (id.length() == 1 && id[0] == OUTPUT_LETTERS[k]) o = k;
    if (o < 0) return sendError("strand must be A to F");
    StrandCfg c = strandCfg[o];
    if (server.hasArg("count")) {
      int n = server.arg("count").toInt();
      if (n < 1 || n > MAX_LEDS) return sendError("a strand needs 1 to 400 lights");
      if (n != counts[o]) { counts[o] = n; countChanged = true; }
    }
    if (server.hasArg("name")) {
      String n = server.arg("name");
      int k = 0;
      for (unsigned int i = 0; i < n.length() && k < STRAND_NAME_LEN - 1; i++) {
        char ch = n[i];
        if (ch < 32 || ch > 126) continue;
        if (ch == '"' || ch == '\\') ch = '\'';
        c.name[k++] = ch;
      }
      while (k > 0 && c.name[k - 1] == ' ') k--;
      c.name[k] = 0;
    }
    if (server.hasArg("type")) {
      String t = server.arg("type");
      int found = -1;
      for (int k = 0; k <= STRAND_RADIO; k++) if (t == TYPE_NAMES[k]) found = k;
      if (found < 0) return sendError("type must be off, wired or radio");
      c.type = found;
    }
    if (server.hasArg("pin")) {
      int pin = server.arg("pin").toInt();
      if (!pinAllowed(pin)) return sendError("that pin can't be used for lights");
      c.pin = pin;
    }
    if (server.hasArg("order")) {
      String v = server.arg("order");
      v.toUpperCase();
      int found = -1;
      for (int k = 0; k <= ORDER_BGR; k++) if (v == ORDER_NAMES[k]) found = k;
      if (found < 0) return sendError("color order must be RGB, RBG, GRB, GBR, BRG or BGR");
      c.order = found;
    }
    if (c.type == STRAND_WIRED) {
      for (int k = 0; k < NUM_OUTPUTS; k++) {
        if (k != o && strandCfg[k].type == STRAND_WIRED && strandCfg[k].pin == c.pin) {
          static char msg[64];
          snprintf(msg, sizeof(msg), "pin %u is already used by strand %c", c.pin, OUTPUT_LETTERS[k]);
          return sendError(msg);
        }
      }
    }
    bool activeChanged = (c.type != STRAND_OFF) != strandActive(o);
    strandCfg[o] = c;
    saveStrandCfg();
    if (activeChanged && !countChanged) strandsChanged();
  }
  // the board's own RGB LED
  if (server.hasArg("boardLed") || server.hasArg("boardLedLight") || server.hasArg("boardLedPin")) {
    BoardLedCfg b = boardLed;
    if (server.hasArg("boardLed")) {
      String v = server.arg("boardLed");
      v.toUpperCase();
      b.strand = -1;
      for (int k = 0; k < NUM_OUTPUTS; k++) if (v.length() == 1 && v[0] == OUTPUT_LETTERS[k]) b.strand = k;
      if (b.strand < 0 && v != "OFF" && v != "") return sendError("boardLed must be a strand letter or off");
    }
    if (server.hasArg("boardLedLight")) b.light = constrain(server.arg("boardLedLight").toInt(), 1, MAX_LEDS);
    if (server.hasArg("boardLedPin")) {
      int pin = server.arg("boardLedPin").toInt();
      if (!boardLedPinAllowed(pin)) return sendError("the board LED is on GPIO 48 or 38");
      b.pin = pin;
    }
    boardLed = b;
    saveStrandCfg();
  }
  if (countChanged) {
    stopLocate();
    setLedCounts(counts);
  }
  sendState();
}
