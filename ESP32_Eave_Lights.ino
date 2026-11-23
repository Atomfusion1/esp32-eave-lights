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
//   led_output.ino    power caps + sending frames to the strands
//   test_pattern.ino  red/green/blue/white/off test cycle
//
// All #defines and shared globals live in this file so every tab can use them.
//
// Stage 1: LED baseline. No Wi-Fi yet.

#include <FastLED.h>

#define FW_VERSION "0.1.2"

// ---------- Outputs ----------
#define NUM_LEDS        65
#define PIN_OUTPUT_A    8   // tested
#define PIN_OUTPUT_B    9   // planned, not yet tested
#define ENABLE_OUTPUT_B 0   // set to 1 once strand B is wired to GPIO 9
#define NUM_OUTPUTS     2

// ---------- Power caps (from one-strand measurements at 24 V) ----------
#define CAP_RED         240
#define CAP_GREEN       240
#define CAP_BLUE        240
#define CAP_CHANNEL_SUM 400  // white ~133 each. Not yet measured (white 100 each = ~1.1 A per strand)

// What effects draw into (full range, uncapped).
CRGB frame[NUM_OUTPUTS][NUM_LEDS];
// What actually gets sent to the LEDs (after power caps).
CRGB out[NUM_OUTPUTS][NUM_LEDS];

void setup() {
  // Send OFF before anything else so the power-up white ends as soon as possible.
  initOutputs();

  Serial.begin(115200);
  delay(1000);  // give USB serial time to connect; lights are already off

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
}

void loop() {
  runTestPattern();
}
