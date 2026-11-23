// ESP32-S3 Eave Lights — controller firmware
//
// Arduino IDE board settings (Tools menu):
//   Board: ESP32S3 Dev Module
//   USB CDC On Boot: Enabled     (otherwise Serial Monitor shows nothing over USB-C)
//   Flash Size: 16MB
//   PSRAM: OPI PSRAM
//
// Stage 1: LED baseline. No Wi-Fi yet. Steps through red, green, blue, white, off.

#include <FastLED.h>

#define FW_VERSION "0.1.0"

// ---------- Outputs ----------
#define NUM_LEDS        65
#define PIN_OUTPUT_A    8   // tested
#define PIN_OUTPUT_B    9   // planned, not yet tested
#define ENABLE_OUTPUT_B 0   // set to 1 once strand B is wired to GPIO 9
#define NUM_OUTPUTS     2

// ---------- Power caps (from one-strand measurements at 24 V) ----------
#define CAP_RED         220
#define CAP_GREEN       240
#define CAP_BLUE        240
#define CAP_CHANNEL_SUM 300  // white at 100+100+100 measured ~1.1 A per strand

// What effects draw into (full range, uncapped).
CRGB frame[NUM_OUTPUTS][NUM_LEDS];
// What actually gets sent to the LEDs (after power caps).
CRGB out[NUM_OUTPUTS][NUM_LEDS];

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

// Applies caps to every pixel and sends the frame to all outputs.
void showFrame() {
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    for (int i = 0; i < NUM_LEDS; i++) {
      out[o][i] = capPixel(frame[o][i]);
    }
  }
  FastLED.show();
}

void setup() {
  // Send OFF before anything else so the power-up white ends as soon as possible.
  FastLED.addLeds<WS2811, PIN_OUTPUT_A, RBG>(out[0], NUM_LEDS);
#if ENABLE_OUTPUT_B
  FastLED.addLeds<WS2811, PIN_OUTPUT_B, RBG>(out[1], NUM_LEDS);
#endif
  FastLED.setBrightness(255);  // brightness is limited by capPixel() instead
  FastLED.clear(true);
  delay(5);
  FastLED.clear(true);         // second OFF frame in case the first was missed

  Serial.begin(115200);
  delay(1000);                 // give USB serial time to connect; lights are already off

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

struct TestStep {
  CRGB color;
  const char* name;
};

const TestStep TEST_STEPS[] = {
  {CRGB::Red,   "RED"},
  {CRGB::Green, "GREEN"},
  {CRGB::Blue,  "BLUE"},
  {CRGB::White, "WHITE"},
  {CRGB::Black, "OFF"},
};
const int NUM_TEST_STEPS = sizeof(TEST_STEPS) / sizeof(TEST_STEPS[0]);
const uint32_t STEP_MS = 2000;

void loop() {
  static int step = -1;
  static uint32_t lastStepMs = 0;
  uint32_t now = millis();

  // Non-blocking: loop() keeps running between steps (needed once Wi-Fi is added).
  if (step < 0 || now - lastStepMs >= STEP_MS) {
    step = (step + 1) % NUM_TEST_STEPS;
    lastStepMs = now;

    for (int o = 0; o < NUM_OUTPUTS; o++) {
      fill_solid(frame[o], NUM_LEDS, TEST_STEPS[step].color);
    }
    showFrame();

    CRGB sent = out[0][0];
    Serial.printf("%-5s -> sent R%d G%d B%d\n", TEST_STEPS[step].name, sent.r, sent.g, sent.b);
  }
}
