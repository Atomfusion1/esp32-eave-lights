#include <FastLED.h>

#define DATA_PIN 8
#define NUM_LEDS 65
#define BRIGHTNESS 80

CRGB leds[NUM_LEDS];

void setup() {
  Serial.begin(115200);
  delay(1000);

  FastLED.addLeds<WS2811, DATA_PIN, RBG>(
    leds, NUM_LEDS
  );

  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear(true);

  Serial.println("ESP32-S3 3.3V DATA TEST");
  Serial.println("GPIO 8 | 65 LEDs | WS2811 | RBG");
}

void showColor(CRGB color, const char* name) {
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();

  Serial.print("Color: ");
  Serial.println(name);

  delay(2000);
}

void loop() {
  showColor(CRGB::Red,   "RED");
  showColor(CRGB::Green, "GREEN");
  showColor(CRGB::Blue,  "BLUE");
  showColor(CRGB::White, "WHITE");
  showColor(CRGB::Black, "OFF");
}