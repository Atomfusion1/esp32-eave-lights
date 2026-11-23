// ARDUINO NANO reference sketch (not ESP32). Known working on one 65-puck strand.
// DATA_PIN 6 = Nano D6 (5 V logic). The ESP32-S3 was tested separately on GPIO 8.
// Per-color brightness caps used for the current measurements in the handoff doc.

#include <FastLED.h>

#define DATA_PIN 6
#define NUM_LEDS 65
#define FRAME_DELAY 10

// Individual maximum brightness (0-255)
#define MAX_RED    220
#define MAX_GREEN  240
#define MAX_BLUE   240
#define MAX_WHITE  100

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);

  // Corrected color order for your LEDs
  FastLED.addLeds<WS2811, DATA_PIN, RBG>(
    leds, NUM_LEDS
  );

  FastLED.setBrightness(255);
  FastLED.clear(true);

  Serial.println("65 LED RGB + White Fade Test");
}

void fadeColor(CRGB color, uint8_t maxLevel,
               const char* name) {

  Serial.print("Starting color: ");
  Serial.print(name);
  Serial.print(" | Maximum: ");
  Serial.println(maxLevel);

  // Fade in
  for (int i = 0; i <= 255; i++) {
    uint8_t level = ease8InOutCubic(i);
    level = scale8(level, maxLevel);

    fill_solid(leds, NUM_LEDS, color);
    FastLED.setBrightness(level);
    FastLED.show();

    delay(FRAME_DELAY);
  }

  // Hold at maximum brightness for 1 second
  delay(1000);

  // Fade out
  for (int i = 255; i >= 0; i--) {
    uint8_t level = ease8InOutCubic(i);
    level = scale8(level, maxLevel);

    FastLED.setBrightness(level);
    FastLED.show();

    delay(FRAME_DELAY);
  }

  FastLED.setBrightness(0);
  FastLED.show();

  delay(500);
}

void loop() {
  fadeColor(CRGB::Red,   MAX_RED,   "RED");
  fadeColor(CRGB::Green, MAX_GREEN, "GREEN");
  fadeColor(CRGB::Blue,  MAX_BLUE,  "BLUE");
  fadeColor(CRGB::White, MAX_WHITE, "WHITE");
}
