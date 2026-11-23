// ESP32-S3 Eave Lights — controller firmware (main tab)
//
// Arduino IDE board settings (Tools menu) — see AGENTS.md:
//   Board: ESP32S3 Dev Module
//   USB CDC On Boot: Disabled    (COM13 is the CH343 USB-serial chip, Serial = UART0)
//   Flash Size: 16MB
//   Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
//   PSRAM: OPI PSRAM
//
// Tabs (the IDE joins them after this file, in alphabetical order):
//   effects.ino         draws the current mode into frame[] each frame
//   led_output.ino      strand setup, power caps, showFrame()
//   settings_store.ino  saves/loads the light state in flash (survives power cuts)
//   test_pattern.ino    red/green/blue/white/off test cycle
//   web_api.ino         web page + JSON API on port 80
//   wifi.ino            home Wi-Fi with hotspot fallback, mDNS name
//
// All #defines, #includes, shared types and globals live in this file so every tab can use them.
//
// Stage 2: Wi-Fi control. Copy secrets.example.h to secrets.h and fill in your Wi-Fi.

#include <FastLED.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <Preferences.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#error "Missing secrets.h: copy secrets.example.h to secrets.h and fill in your Wi-Fi details"
#endif

#define FW_VERSION "0.4.0"
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
};
#define MODE_COUNT 3

struct LightState {
  bool    on;
  CRGB    color;
  uint8_t brightness;   // 0-255, applied before the power caps
  Mode    mode;
  uint8_t fadeSeconds;  // fade mode: seconds per breath (FADE_SECONDS_MIN-FADE_SECONDS_MAX)
  uint8_t fadeMin;      // fade mode: lowest level 0-255 (64 = 25%)
};

#define FADE_SECONDS_MIN 2
#define FADE_SECONDS_MAX 60

// One state per output (strand). Defaults used on first boot (nothing saved yet).
LightState states[NUM_OUTPUTS] = {
  { false, CRGB(255, 140, 40), 128, MODE_SOLID, 5, 64 },
  { false, CRGB(255, 140, 40), 128, MODE_SOLID, 5, 64 },
};

// true = both strands show the same thing; every change is applied to all outputs.
bool linked = true;

const char  OUTPUT_LETTERS[NUM_OUTPUTS] = { 'A', 'B' };
const int   OUTPUT_PINS[NUM_OUTPUTS]    = { PIN_OUTPUT_A, PIN_OUTPUT_B };
const bool  OUTPUT_ENABLED[NUM_OUTPUTS] = { true, ENABLE_OUTPUT_B };

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

  loadState();
  startWifi();
  startWeb();
}

void loop() {
  handleWifi();
  handleWeb();
  renderFrame();
  handleStateSave();
}
