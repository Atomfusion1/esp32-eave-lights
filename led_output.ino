// led_output — sets up the strands, applies power caps, sends frames.
//
// Strand lengths (ledCount[]) are set on the page and saved. At power-up the strands
// are registered at MAX_LEDS and cleared, so any lights past the saved length are sent
// OFF too; then applyLedCounts() trims each strand to its real length.

CLEDController* controllers[NUM_OUTPUTS] = { nullptr };

// Registers the strands with FastLED and sends OFF twice.
// Called first in setup() so the power-up white ends as soon as possible.
void initOutputs() {
  controllers[0] = &FastLED.addLeds<WS2811, PIN_OUTPUT_A, RBG>(out[0], MAX_LEDS);
#if ENABLE_OUTPUT_B
  controllers[1] = &FastLED.addLeds<WS2811, PIN_OUTPUT_B, RBG>(out[1], MAX_LEDS);
#endif
  FastLED.setBrightness(255);  // brightness is limited by capPixel() instead
  FastLED.clear(true);
  delay(5);
  FastLED.clear(true);         // second OFF frame in case the first was missed
}

// Tells FastLED each strand's current length.
void applyLedCounts() {
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    if (controllers[o]) controllers[o]->setLeds(out[o], ledCount[o]);
  }
}

// Changes strand lengths (from the page). Sends OFF over the old lengths first, so
// lights past a new, shorter end don't keep their last color. Then rebuilds
// everything that depends on the lengths.
void setLedCounts(uint16_t countA, uint16_t countB) {
  for (int o = 0; o < NUM_OUTPUTS; o++) fill_solid(out[o], MAX_LEDS, CRGB(0, 0, 0));
  FastLED.show();
  ledCount[0] = constrain(countA, 1, MAX_LEDS);
  ledCount[1] = constrain(countB, 1, MAX_LEDS);
  applyLedCounts();
  for (int o = 0; o < NUM_OUTPUTS; o++) fill_solid(frame[o], MAX_LEDS, CRGB(0, 0, 0));
  initPaths();
  for (int z = 0; z < MAX_ZONES; z++) rebuildZone(z);
  rebuildAllZonePaths();
  markStateChanged();
  Serial.printf("Strand lengths: A %u, B %u\n", ledCount[0], ledCount[1]);
}

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

// Applies caps to every pixel in frame[] and sends the result to all outputs.
void showFrame() {
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    for (int i = 0; i < ledCount[o]; i++) {
      out[o][i] = capPixel(frame[o][i]);
    }
  }
  FastLED.show();
}
