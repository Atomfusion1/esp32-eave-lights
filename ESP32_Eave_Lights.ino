// ESP32-S3 Eave Lights — controller firmware (main tab)
//
// Arduino IDE board settings (Tools menu) — see AGENTS.md:
//   Board: ESP32S3 Dev Module
//   USB CDC On Boot: Disabled    (COM13 is the CH343 USB-serial chip, Serial = UART0)
//   Flash Size: 16MB
//   Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)   <- zones/looks are stored there
//   PSRAM: OPI PSRAM
//
// Tabs (the IDE joins them after this file, in alphabetical order):
//   effects.ino         draws each channel's mode into frame[] each frame, layers, locate
//   files.ino           house photos + layout.json for the House view editor (/files)
//   led_output.ino      strand setup, power caps, showFrame()
//   locate.ino          "Find a light": lights chosen lights only, auto-off
//   paths.ino           Whole house path, and paths made from zones (/paths.bin)
//   presets.ino         saved looks: named effect settings, stored in /presets.bin
//   settings_store.ino  strand/house settings in Preferences; mounts the FAT storage
//   test_pattern.ino    red/green/blue/white/off test cycle
//   web_api.ino         JSON API on port 80
//   web_page.h          the control page (HTML/CSS/JS) — a .h on purpose, see below
//   editor_page.h       the House view editor page (photos, light placement, live glow)
//   wifi.ino            home Wi-Fi with hotspot fallback, mDNS name
//   zones.ino           zones: named light ranges with their own settings, /zones.bin
//
// All #defines, #includes, shared types and globals live in this file so every tab can use them.
//
// Wi-Fi: copy secrets.example.h to secrets.h and fill in your Wi-Fi.

#include <FastLED.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Preferences.h>
#include <FFat.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h: copy secrets.example.h to secrets.h and fill in your Wi-Fi details"
#endif

#define FW_VERSION "0.11.0"
#define HOSTNAME   "eavelights"   // -> http://eavelights.local

// ---------- Outputs ----------
#define NUM_LEDS        65
#define PIN_OUTPUT_A    8   // tested
#define PIN_OUTPUT_B    9   // pin tested 2026-11-23 with strand A moved over; strand B itself untested
#define ENABLE_OUTPUT_B 1   // 1 = drive GPIO 9 too
#define NUM_OUTPUTS     2

// ---------- Power caps (from one-strand measurements at 24 V) ----------
#define CAP_RED         240
#define CAP_GREEN       240
#define CAP_BLUE        240
#define CAP_CHANNEL_SUM 400  // white ~133 each = measured 1.26 A per strand

// What effects draw into (full range, uncapped).
CRGB frame[NUM_OUTPUTS][NUM_LEDS];
// What actually gets sent to the LEDs (after power caps).
CRGB out[NUM_OUTPUTS][NUM_LEDS];

// ---------- Light state (set from the web page, saved in flash) ----------
enum Mode : uint8_t {
  MODE_SOLID = 0,   // steady color
  MODE_FADE  = 1,   // color breathes in and out
  MODE_TEST  = 2,   // red/green/blue/white/off cycle, ignores color and brightness
  MODE_CHASE = 3,   // blocks of `size` lit lights with equal gaps, moving along the strand
  MODE_COMET = 4,   // one bright head with a `size`-long fading tail, travels the strand and wraps
};
#define MODE_COUNT 5

struct LightState {
  bool    on;
  CRGB    color;
  uint8_t brightness;   // 0-255, applied before the power caps
  Mode    mode;
  uint8_t fadeSeconds;  // fade mode: seconds per breath (FADE_SECONDS_MIN-FADE_SECONDS_MAX)
  uint8_t fadeMin;      // fade mode: lowest level 0-255 (64 = 25%)
  uint8_t speed;        // chase/comet: lights per second (SPEED_MIN-SPEED_MAX)
  uint8_t size;         // chase: lit block length; comet: tail length (SIZE_MIN-SIZE_MAX)
  bool    reverse;      // chase/comet: false = moves from light 1 toward light 65
  bool    bgOn;         // chase/comet: true = unlit lights show bgColor instead of off
  CRGB    bgColor;      // chase/comet background, also scaled by brightness
};

#define FADE_SECONDS_MIN 2
#define FADE_SECONDS_MAX 60
#define SPEED_MIN 1
#define SPEED_MAX 100
#define SIZE_MIN  1
#define SIZE_MAX  30

const char  OUTPUT_LETTERS[NUM_OUTPUTS] = { 'A', 'B' };
const int   OUTPUT_PINS[NUM_OUTPUTS]    = { PIN_OUTPUT_A, PIN_OUTPUT_B };
const bool  OUTPUT_ENABLED[NUM_OUTPUTS] = { true, ENABLE_OUTPUT_B };

// ---------- Paths ----------
// A path is an ordered list of physical lights. An effect on a path draws along it as
// one line, crossing strands wherever the path does.
struct PathPoint {
  uint8_t  output;   // 0 = A, 1 = B
  uint16_t index;    // 0-based light on that strand (light 1 = index 0)
};

// "Whole house": B65 -> B1, then A1 -> A65 (built in paths.ino).
#define HOUSE_PATH_LEN (NUM_OUTPUTS * NUM_LEDS)
PathPoint housePath[HOUSE_PATH_LEN];

// ---------- Zones ----------
// A zone is a name plus up to MAX_ZONE_RANGES light ranges, e.g. "Porch" = A1-12.
// Its lights form one line in range order; a range with first > last runs backwards.
#define MAX_ZONES        12
#define MAX_ZONE_RANGES  6
#define NAME_LEN         24   // zone and saved-look names, including the ending 0

struct ZoneRange {
  uint8_t output;   // 0 = A, 1 = B
  uint8_t first;    // 0-based, inclusive
  uint8_t last;     // 0-based, inclusive; first > last = runs backwards
};

struct ZoneDef {
  bool      used;
  char      name[NAME_LEN];
  uint8_t   rangeCount;
  ZoneRange ranges[MAX_ZONE_RANGES];
};

ZoneDef   zones[MAX_ZONES];
PathPoint zonePoints[MAX_ZONES][HOUSE_PATH_LEN];  // each zone's lights in line order
uint16_t  zoneLen[MAX_ZONES];                     // lights in each zone (0 if unused)

// ---------- Paths made from zones ----------
// A zone path is a name plus an ordered list of zones; each step can run its zone
// reversed and add a gap of invisible lights after it (the effect "jumps").
// Leaving a zone out of the path skips it.
#define MAX_PATHS        8
#define MAX_PATH_STEPS   12
#define MAX_PATH_GAP     30
#define MAX_PATH_LEN     (HOUSE_PATH_LEN + MAX_PATH_STEPS * MAX_PATH_GAP)
#define VIRTUAL_OUTPUT   0xFF   // PathPoint.output for a gap position (no real light)

struct PathStep {
  uint8_t zone;       // zone slot
  uint8_t reverse;    // 1 = run this zone's lights backwards
  uint8_t gapAfter;   // invisible lights after this zone (0-MAX_PATH_GAP)
};

struct PathDef {
  bool     used;
  char     name[NAME_LEN];
  uint8_t  stepCount;
  PathStep steps[MAX_PATH_STEPS];
};

PathDef   pathDefs[MAX_PATHS];
PathPoint pathPoints[MAX_PATHS][MAX_PATH_LEN];   // each path's positions in order
uint16_t  pathLen[MAX_PATHS];                    // positions in each path, gaps included

// ---------- Channels ----------
// A channel is anything with its own LightState that draws effects:
//   0 = strand A, 1 = strand B, CH_HOUSE = Whole house path, CH_ZONE0 + z = zone z,
//   CH_PATH0 + p = zone path p.
// Drawing order (later covers earlier): strands, Whole house, zone paths, zones
// (each only while on).
#define CH_HOUSE     NUM_OUTPUTS
#define CH_ZONE0     (NUM_OUTPUTS + 1)
#define CH_PATH0     (CH_ZONE0 + MAX_ZONES)
#define NUM_CHANNELS (CH_PATH0 + MAX_PATHS)

// Defaults used on first boot (nothing saved yet). Zone states are set in initZones().
LightState states[NUM_CHANNELS] = {
  { false, CRGB(255, 140, 40), 128, MODE_SOLID, 5, 64, 10, 5, false, false, CRGB(16, 32, 64) },  // strand A
  { false, CRGB(255, 140, 40), 128, MODE_SOLID, 5, 64, 10, 5, false, false, CRGB(16, 32, 64) },  // strand B
  { false, CRGB(255, 140, 40), 128, MODE_COMET, 5, 64, 20, 8, false, false, CRGB(16, 32, 64) },  // Whole house
};
// A new zone starts as steady warm white, off.
const LightState ZONE_DEFAULT_STATE =
  { false, CRGB(255, 160, 60), 160, MODE_SOLID, 5, 64, 10, 5, false, false, CRGB(16, 32, 64) };
// A new zone path starts as an orange comet, off.
const LightState PATH_DEFAULT_STATE =
  { false, CRGB(255, 140, 40), 128, MODE_COMET, 5, 64, 20, 8, false, false, CRGB(16, 32, 64) };

// ---------- Saved looks (presets) ----------
// A named copy of a channel's settings (everything except on/off), e.g. "Xmas chase".
#define MAX_PRESETS 32

struct Preset {
  bool       used;
  char       name[NAME_LEN];
  LightState look;
};

Preset presets[MAX_PRESETS];

// ---------- Find a light ----------
// While active, only these lights are lit (first light of each range green, rest white).
bool      locateActive = false;
uint8_t   locateCount = 0;
ZoneRange locateRanges[MAX_ZONE_RANGES];
uint32_t  locateStartMs = 0;

// The web pages (HTML/CSS/JS). Kept in .h files, not .ino tabs, because the IDE
// scans .ino tabs for functions and mistakes the pages' JavaScript for C++.
#include "web_page.h"      // control page, "/"
#include "editor_page.h"   // House view editor, "/editor"

// The web server; used by web_api.ino and files.ino.
WebServer server(80);

// ---------- Storage ----------
bool storageOk = false;   // true once the FAT storage area is mounted

// Start of /zones.bin and /presets.bin. A file whose recordSize doesn't match the
// current structs (firmware changed them) is ignored instead of read as garbage.
struct FileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t recordSize;
  uint16_t count;
};

void setup() {
  // Send OFF before anything else so the power-up white ends as soon as possible.
  initOutputs();

  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("ESP32-S3 Eave Lights v" FW_VERSION);
  Serial.printf("Output A: GPIO %d, %d LEDs\n", PIN_OUTPUT_A, NUM_LEDS);
#if ENABLE_OUTPUT_B
  Serial.printf("Output B: GPIO %d, %d LEDs\n", PIN_OUTPUT_B, NUM_LEDS);
#else
  Serial.println("Output B: disabled");
#endif
  Serial.printf("Flash: %u MB, PSRAM: %u MB\n",
                ESP.getFlashChipSize() / (1024 * 1024),
                ESP.getPsramSize() / (1024 * 1024));

  initPaths();
  initZones();
  initZonePaths();
  loadState();
  startStorage();
  startFiles();
  loadZones();
  loadZonePaths();   // after zones: paths are built from them
  loadPresets();
  startWifi();
  startWeb();
}

void loop() {
  handleWifi();
  handleWeb();
  handleLocateTimeout();
  renderFrame();
  handleStateSave();
}
