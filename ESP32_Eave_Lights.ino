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
//   backup.ino          backup (file list, raw file access) and restore of the whole setup
//   effects.ino        draws each channel's mode into frame[] each frame, layers, locate
//   files.ino           house photos + layout.json for the House view editor (/files)
//   led_output.ino      strand setup, power caps, showFrame()
//   locate.ino          "Find a light": lights chosen lights only, auto-off
//   ota.ino             wireless firmware updates (Arduino IDE network port + web upload)
//   paths.ino           Whole house path, and paths made from zones (/paths.bin)
//   patterns.ino        custom painted patterns (a color per light), /patterns.bin
//   animations.ino      animations: looping steps that move/fade paintings or colors, /anims.bin
//   presets.ino         saved looks: named effect settings, stored in /presets.bin
//   scenes.ino          scenes: whole-house snapshots of every channel, /scenes.bin
//   schedule.ino        playlists of scenes with times, yearly date ranges, internet clock
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
#include <ArduinoOTA.h>
#include <Update.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h: copy secrets.example.h to secrets.h and fill in your Wi-Fi details"
#endif

// Password for wireless firmware updates. Set OTA_PASSWORD in secrets.h;
// older secrets.h files without it fall back to the hotspot password.
#ifndef OTA_PASSWORD
#define OTA_PASSWORD AP_PASSWORD
#endif

#define FW_VERSION "0.31.0"
#define HOSTNAME   "eavelights"   // -> http://eavelights.local

// ---------- Outputs ----------
// Each strand's length is set on the page (Setup > Strand lengths) and saved;
// arrays are sized for MAX_LEDS so lengths can change without new firmware.
#define MAX_LEDS        300   // per strand
#define DEFAULT_LEDS_A  67    // user, 2026-11-23: strand A has 67 lights
#define DEFAULT_LEDS_B  65
#define PIN_OUTPUT_A    8   // tested
#define PIN_OUTPUT_B    9   // pin tested 2026-11-23 with strand A moved over; strand B itself untested
#define ENABLE_OUTPUT_B 1   // 1 = drive GPIO 9 too
#define NUM_OUTPUTS     2

// ---------- Power caps (from one-strand measurements at 24 V) ----------
#define CAP_RED         240
#define CAP_GREEN       240
#define CAP_BLUE        240
#define CAP_CHANNEL_SUM 400  // white ~133 each = measured 1.26 A per strand

// Lights on each strand right now (1..MAX_LEDS).
uint16_t ledCount[NUM_OUTPUTS] = { DEFAULT_LEDS_A, DEFAULT_LEDS_B };
#define MAX_TOTAL_LEDS (NUM_OUTPUTS * MAX_LEDS)

// What effects draw into (full range, uncapped).
CRGB frame[NUM_OUTPUTS][MAX_LEDS];
// What actually gets sent to the LEDs (after power caps).
CRGB out[NUM_OUTPUTS][MAX_LEDS];

// ---------- Light state (set from the web page, saved in flash) ----------
// Numbers are stored in saved files: only ever add new modes at the end.
enum Mode : uint8_t {
  MODE_SOLID        = 0,   // steady color
  MODE_FADE         = 1,   // color breathes in and out
  MODE_TEST         = 2,   // red/green/blue/white/off cycle, ignores color and brightness
  MODE_CHASE        = 3,   // blocks of `size` lit lights with equal gaps, moving
  MODE_COMET        = 4,   // bright head with a `size`-long fading tail, travels and wraps
  MODE_RAINBOW      = 5,   // rainbow scrolling along the line; size*5 lights per rainbow
  MODE_RAINBOWCYCLE = 6,   // all lights one color, cycling through the rainbow
  MODE_WIPE         = 7,   // fills the line light by light with each color in turn
  MODE_GRADIENT     = 8,   // blend across the colors from one end to the other; speed > 1 drifts
  MODE_STRIPES      = 9,   // bands of `size` lights in each color; speed > 1 slides
  MODE_ALTERNATE    = 10,  // groups of `size` lights alternate colors, swapping on a beat
  MODE_COLORCYCLE   = 11,  // whole line fades through the colors, fadeSeconds per color
  MODE_TWINKLE      = 12,  // lights twinkle in the colors over the background
  MODE_BOUNCE       = 13,  // comet sweeping back and forth
  MODE_CENTER       = 14,  // ripples from the center (controller on Whole house) out, or in
  MODE_METEOR       = 15,  // comet with a broken, sparkly trail
  MODE_RUNNING      = 16,  // smooth brightness wave rolling along the line
  MODE_SPARKLE      = 17,  // quick flashes (color 2, or white) over color 1
  MODE_FIRE         = 18,  // candle-like flicker in color 1
  MODE_LIGHTNING    = 19,  // occasional flashes in color 1, dark (or background) between
  MODE_STROBE       = 20,  // on/off flashing in color 1
  MODE_CUSTOM       = 21,  // each light painted by hand: painting number `size` (1-30)
  MODE_ANIM         = 22,  // animation number `size` (1-30): steps that move / fade frames
  MODE_SWAY         = 23,  // stripes of `size` lights glide one stripe over and back (speed)
  MODE_BULBS        = 24,  // multicolor bulbs (`size` lights per color), each gently twinkling
  MODE_GLITTER      = 25,  // multicolor bulbs, steady, with quick white glints (size = how many)
  MODE_BLINKSHIFT   = 26,  // multicolor bulbs blink off, back on one color further along
  MODE_LEAVES       = 27,  // leaves drifting along, swinging back a little now and then
};
#define MODE_COUNT 28

// ---------- Custom patterns ----------
// A color per physical light, painted on the page; a channel in MODE_CUSTOM shows
// pattern number `size`. Black = off (or the background). Saved in /patterns.bin.
// A library of up to MAX_PATTERNS named paintings (size = painting number 1-30, so it
// stays within SIZE_MAX). Kept in PSRAM (allocated in loadPatterns()).
#define MAX_PATTERNS 30
CRGB (*patterns)[NUM_OUTPUTS][MAX_LEDS] = nullptr;   // patterns[p][output][light]
// Unapplied paint (page in "on Apply" mode): edits go to paintDraft, which only the
// preview shows, until Apply (commit) or Discard. draftPattern -1 = no draft.
int8_t draftPattern = -1;
CRGB   paintDraft[NUM_OUTPUTS][MAX_LEDS];
bool   previewPass = false;   // true while preview.ino draws the preview frame

#define MAX_COLORS 5   // colors for multi-color effects: color + up to 4 extra

struct LightState {
  bool    on;
  CRGB    color;        // color 1
  uint8_t brightness;   // 0-255, applied before the power caps
  Mode    mode;
  uint8_t fadeSeconds;  // fade: seconds per breath; color cycle: seconds per color
  uint8_t fadeMin;      // fade mode: lowest level 0-255 (64 = 25%)
  uint8_t speed;        // moving effects: speed (SPEED_MIN-SPEED_MAX), meaning per effect
  uint8_t size;         // moving effects: length/width/density (SIZE_MIN-SIZE_MAX)
  uint8_t reverse : 1;  // direction (center: 1 = inward)
  uint8_t jitter  : 7;  // randomness 0-100 %: speed wanders up and down by this much (v0.31.0;
                        // shares the old reverse byte, so saved files keep their layout)
  bool    bgOn;         // true = unlit lights show bgColor instead of off
  CRGB    bgColor;      // background, also scaled by brightness
  uint8_t colorCount;   // colors used by multi-color effects (1-MAX_COLORS): color + extra
  CRGB    extra[MAX_COLORS - 1];  // colors 2-5
};
static_assert(sizeof(LightState) == 28, "LightState size is part of every saved .bin file");

// LightState before v0.15.0 (no color list). Only used to convert old saved files.
struct LightStateV1 {
  bool    on;
  CRGB    color;
  uint8_t brightness;
  Mode    mode;
  uint8_t fadeSeconds, fadeMin, speed, size;
  bool    reverse, bgOn;
  CRGB    bgColor;
};

// Colors 2-5 for anything that hasn't chosen its own: white, red, green, blue.
#define DEFAULT_EXTRA_COLORS 2, { CRGB(255, 255, 255), CRGB(255, 0, 0), CRGB(0, 255, 0), CRGB(0, 0, 255) }

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

// "Whole house": last B light -> B1, then A1 -> last A light (built in paths.ino).
PathPoint housePath[MAX_TOTAL_LEDS];
uint16_t  houseLen = 0;

// ---------- Zones ----------
// A zone is a name plus up to MAX_ZONE_RANGES light ranges, e.g. "Porch" = A1-12.
// Its lights form one line in range order; a range with first > last runs backwards.
#define MAX_ZONES        12
#define MAX_ZONE_RANGES  6
#define NAME_LEN         24   // zone and saved-look names, including the ending 0

struct ZoneRange {
  uint8_t  output;  // 0 = A, 1 = B
  uint16_t first;   // 0-based, inclusive
  uint16_t last;    // 0-based, inclusive; first > last = runs backwards
};

struct ZoneDef {
  bool      used;
  char      name[NAME_LEN];
  uint8_t   rangeCount;
  ZoneRange ranges[MAX_ZONE_RANGES];
};

ZoneDef   zones[MAX_ZONES];
PathPoint (*zonePoints)[MAX_TOTAL_LEDS] = nullptr;  // [MAX_ZONES]: each zone's lights in line order (PSRAM)
uint16_t  zoneLen[MAX_ZONES];                     // lights in each zone (0 if unused)

// Zone extras, kept in /zonemeta.bin so zones.bin's format didn't change:
// zoneHidden = "never light" (e.g. the lead-in running down the wall): those lights stay
// dark and are left out of the strand lines and the Whole house path, so effects start
// at the eave. Find a light can still show them. zoneRank = position in the page's lists.
uint8_t  zoneHidden[MAX_ZONES];
uint8_t  zoneRank[MAX_ZONES];
bool     lightHidden[NUM_OUTPUTS][MAX_LEDS];   // built from the hidden zones
uint16_t strandMap[NUM_OUTPUTS][MAX_LEDS];     // strand line position -> light index
uint16_t strandLen[NUM_OUTPUTS];               // visible lights per strand

// ---------- Paths made from zones ----------
// A zone path is a name plus an ordered list of zones; each step can run its zone
// reversed and add a gap of invisible lights after it (the effect "jumps").
// Leaving a zone out of the path skips it.
#define MAX_PATHS        8
#define MAX_PATH_STEPS   12
#define MAX_PATH_GAP     30
#define MAX_PATH_LEN     (MAX_TOTAL_LEDS + MAX_PATH_STEPS * MAX_PATH_GAP)
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
PathPoint (*pathPoints)[MAX_PATH_LEN] = nullptr;    // [MAX_PATHS]: each path's positions in order (PSRAM)
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
  { false, CRGB(255, 140, 40), 128, MODE_SOLID, 5, 64, 10, 5, false, 0, false, CRGB(16, 32, 64), DEFAULT_EXTRA_COLORS },  // strand A
  { false, CRGB(255, 140, 40), 128, MODE_SOLID, 5, 64, 10, 5, false, 0, false, CRGB(16, 32, 64), DEFAULT_EXTRA_COLORS },  // strand B
  { false, CRGB(255, 140, 40), 128, MODE_COMET, 5, 64, 20, 8, false, 0, false, CRGB(16, 32, 64), DEFAULT_EXTRA_COLORS },  // Whole house
};
// A new zone starts as steady warm white, off.
const LightState ZONE_DEFAULT_STATE =
  { false, CRGB(255, 160, 60), 160, MODE_SOLID, 5, 64, 10, 5, false, 0, false, CRGB(16, 32, 64), DEFAULT_EXTRA_COLORS };
// A new zone path starts as an orange comet, off.
const LightState PATH_DEFAULT_STATE =
  { false, CRGB(255, 140, 40), 128, MODE_COMET, 5, 64, 20, 8, false, 0, false, CRGB(16, 32, 64), DEFAULT_EXTRA_COLORS };

// ---------- Animations ----------
// A looping list of steps applied to a "frame": the channel's colors as bands, or a
// painting. Steps move the frame along the line (wrapping around), fade or switch to
// another frame, or hold. A channel in MODE_ANIM plays animation number `size`.
// Saved in /anims.bin (animations.ino).
#define MAX_ANIMS       30      // animation number = size (1-30); a slot exists if it has a name
#define MAX_ANIM_STEPS  16
#define ANIM_MAX_MOVE   60      // lights per move step, either way
#define ANIM_MAX_MS     60000   // per step
enum AnimKind : uint8_t { ANIM_HOLD = 0, ANIM_MOVE = 1, ANIM_FADE = 2, ANIM_CUT = 3 };
struct AnimStep {
  uint8_t  kind;      // AnimKind
  int8_t   lights;    // move: lights to move, + = along the line, - = back
  uint8_t  frame;     // fade/cut: 0 = colors, 1-30 = painting
  uint8_t  steady;    // move: 0 = glide eased (slow start and stop), 1 = glide steady,
                      //       2 = step (hop a whole light at a time, even pace)
  uint16_t ms;        // how long the step takes (cut: how long to hold after switching)
};
struct AnimDef {
  char     name[NAME_LEN];
  uint8_t  startFrame;   // frame at the start of each loop: 0 = colors, 1-30 = painting
  uint8_t  bandWidth;    // colors frame: lights per color band
  uint8_t  stepCount;
  AnimStep steps[MAX_ANIM_STEPS];
};
AnimDef anims[MAX_ANIMS];

// Milliseconds since each channel's effect started (timed effects: twinkle, animations...).
uint32_t effectMs[NUM_CHANNELS] = { 0 };

// ---------- Saved looks (presets) ----------
// A named copy of a channel's settings (everything except on/off), e.g. "Xmas chase".
#define MAX_PRESETS 32

struct Preset {
  bool       used;
  char       name[NAME_LEN];
  LightState look;
};

Preset presets[MAX_PRESETS];

// ---------- Scenes ----------
// A named snapshot of every channel's settings (strands, Whole house, all zones, all
// zone paths, including on/off), e.g. "Xmas". Applying it restores the whole house.
// Zones/paths themselves (their lights) are not part of a scene: all scenes share them.
#define MAX_SCENES 64   // grew from 16 (v0.24) and 40 (v0.26.1); same record format, the file just gets longer

struct Scene {
  bool       used;
  char       name[NAME_LEN];
  LightState states[NUM_CHANNELS];
};

Scene* scenes = nullptr;   // [MAX_SCENES], in PSRAM (allocBigTables)

// ---------- Playlists & calendar ----------
// A playlist ("program") is a list of scenes, each shown for a number of seconds, looping.
// It can have a yearly date range (e.g. Nov 20 - Nov 30); with the calendar on, the
// playlist whose range includes today plays (shortest range wins if several match).
// The date comes from the internet (NTP) in this time zone:
#define TIMEZONE "EST5EDT,M3.2.0,M11.1.0"   // US Eastern (Ohio), with daylight saving
#define MAX_PROGRAMS      12
#define MAX_PROGRAM_STEPS 30   // was 10 before v0.29.0 (programs.bin v1, converted on load)

struct ProgramStep {
  uint8_t  scene;     // scene slot
  uint32_t seconds;   // how long it shows before the next one
};

struct Program {
  bool        used;
  char        name[NAME_LEN];
  uint8_t     stepCount;
  ProgramStep steps[MAX_PROGRAM_STEPS];
  bool        useDates;              // false = only plays when started by hand
  uint8_t     fromMonth, fromDay;    // inclusive, every year
  uint8_t     toMonth, toDay;        // inclusive; before "from" = wraps over New Year
  uint8_t     shuffle;               // 1 = random order each round (v0.30.0; fits in padding)
};

Program programs[MAX_PROGRAMS];

// On/off timer: "All lights on" at the on time, "All lights off" at the off time.
// Times are sunset/sunrise (+ offset minutes, from the location below) or a clock time.
enum TimerMode : uint8_t { TIMER_SUN = 0, TIMER_CLOCK = 1, TIMER_NEVER = 2 };
struct LightTimer {
  bool      enabled;
  TimerMode onMode;      // TIMER_SUN = sunset, TIMER_CLOCK = onClock
  int16_t   onClock;     // minutes after midnight
  int16_t   onOffset;    // minutes added to sunset (-120..120)
  TimerMode offMode;     // TIMER_SUN = sunrise, TIMER_CLOCK = offClock, TIMER_NEVER
  int16_t   offClock;
  int16_t   offOffset;   // minutes added to sunrise
  float     lat, lon;    // for sunset/sunrise; default central Ohio
};
LightTimer lightTimer = { false, TIMER_SUN, 17 * 60, 0, TIMER_SUN, 7 * 60, 0, 40.0f, -82.9f };
bool    scheduleOn    = false;   // follow the calendar (saved)
int8_t  manualProgram = -1;      // playlist started with Play, or -1 (saved)
// Calendar dates no playlist covers: FALLBACK_KEEP = lights stay as they are,
// FALLBACK_OFF = all lights off (and nothing for "All on" to bring back), 0+ = that playlist.
#define FALLBACK_KEEP -1
#define FALLBACK_OFF  -2
int8_t  fallbackProgram = FALLBACK_KEEP;   // saved in Preferences "fallback"

// ---------- All off / All on ----------
// Bit ch set = channel ch was on when "All lights off" was last pressed; "All lights on"
// turns those back on. Saved with the other settings (survives power cuts).
uint32_t allOnMask = 0;

// ---------- Restore ----------
// While a backup is being restored, nothing is saved, so the files being uploaded
// aren't overwritten from memory. Cleared by /api/restore-finish or after a timeout.
bool     savesBlocked   = false;
uint32_t savesBlockedMs = 0;

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
const char*  FILES_DIR      = "/files";            // house photos + layout.json
const size_t FILE_MAX_BYTES = 4UL * 1024 * 1024;   // largest photo / restored file

// Start of /zones.bin and /presets.bin. A file whose recordSize doesn't match the
// current structs (firmware changed them) is ignored instead of read as garbage.
struct FileHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t recordSize;
  uint16_t count;
};

// Big tables go in PSRAM (8 MB) to leave internal RAM for Wi-Fi and the web server
// (about 100 KB). Zeroed like normal globals. Falls back to internal RAM without PSRAM.
void* bigAlloc(size_t bytes) {
  void* p = ps_calloc(1, bytes);
  if (!p) p = calloc(1, bytes);
  if (!p) {
    Serial.printf("Out of memory allocating %u bytes, restarting\n", (unsigned)bytes);
    delay(2000);
    ESP.restart();
  }
  return p;
}

void allocBigTables() {
  zonePoints = (PathPoint(*)[MAX_TOTAL_LEDS])bigAlloc(sizeof(PathPoint) * MAX_ZONES * MAX_TOTAL_LEDS);
  pathPoints = (PathPoint(*)[MAX_PATH_LEN])bigAlloc(sizeof(PathPoint) * MAX_PATHS * MAX_PATH_LEN);
  scenes     = (Scene*)bigAlloc(sizeof(Scene) * MAX_SCENES);
}

void setup() {
  // Send OFF before anything else so the power-up white ends as soon as possible.
  initOutputs();

  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("ESP32-S3 Eave Lights v" FW_VERSION);
  Serial.printf("Flash: %u MB, PSRAM: %u MB\n",
                ESP.getFlashChipSize() / (1024 * 1024),
                ESP.getPsramSize() / (1024 * 1024));
  allocBigTables();   // before anything uses zones, paths or scenes
  Serial.printf("Free internal RAM: %u bytes, free PSRAM: %u bytes\n",
                (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL), (unsigned)ESP.getFreePsram());

  initZones();
  initZonePaths();
  loadState();          // includes the strand lengths
  applyLedCounts();     // strands to their saved lengths
  initPaths();          // Whole house path follows the lengths
  Serial.printf("Output A: GPIO %d, %u lights\n", PIN_OUTPUT_A, ledCount[0]);
#if ENABLE_OUTPUT_B
  Serial.printf("Output B: GPIO %d, %u lights\n", PIN_OUTPUT_B, ledCount[1]);
#else
  Serial.println("Output B: disabled");
#endif
  startStorage();
  startFiles();
  loadZones();
  loadZonePaths();   // after zones: paths are built from them
  loadPresets();
  loadScenes();
  loadPrograms();
  loadPatterns();
  loadAnims();
  startWifi();
  startWeb();
  startOta();
}

void loop() {
  handleWifi();
  handleOta();
  handleWeb();
  handleLocateTimeout();
  handleSchedule();
  renderFrame();
  handleStateSave();
  handlePatternSave();
}
