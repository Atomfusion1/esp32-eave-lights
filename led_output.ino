// led_output — sets up the strands, applies power caps, sends frames.

// Registers the strands with FastLED and sends OFF twice.
// Called first in setup() so the power-up white ends as soon as possible.
void initOutputs() {
  FastLED.addLeds<WS2811, PIN_OUTPUT_A, RBG>(out[0], NUM_LEDS);
#if ENABLE_OUTPUT_B
  FastLED.addLeds<WS2811, PIN_OUTPUT_B, RBG>(out[1], NUM_LEDS);
#endif
  FastLED.setBrightness(255);  // brightness is limited by capPixel() instead
  FastLED.clear(true);
  delay(5);
  FastLED.clear(true);         // second OFF frame in case the first was missed
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
    for (int i = 0; i < NUM_LEDS; i++) {
      out[o][i] = capPixel(frame[o][i]);
    }
  }
  FastLED.show();
}
