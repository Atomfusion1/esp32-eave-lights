// test_pattern — red, green, blue, white, off cycle for checking colors and measuring current.
// testPatternColor() returns the color for the current step; the step changes every STEP_MS.

struct TestStep {
  CRGB color;
  const char* name;
};

// Explicit full values: FastLED's CRGB::Green is the web color (0,128,0), half brightness.
const TestStep TEST_STEPS[] = {
  {CRGB(255,   0,   0), "RED"},
  {CRGB(  0, 255,   0), "GREEN"},
  {CRGB(  0,   0, 255), "BLUE"},
  {CRGB(255, 255, 255), "WHITE"},
  {CRGB(  0,   0,   0), "OFF"},
};
const int NUM_TEST_STEPS = sizeof(TEST_STEPS) / sizeof(TEST_STEPS[0]);
const uint32_t STEP_MS = 2000;

CRGB testPatternColor() {
  static int step = -1;
  static uint32_t lastStepMs = 0;
  uint32_t now = millis();

  if (step < 0 || now - lastStepMs >= STEP_MS) {
    step = (step + 1) % NUM_TEST_STEPS;
    lastStepMs = now;
    CRGB sent = capPixel(TEST_STEPS[step].color);
    Serial.printf("%-5s -> sent R%d G%d B%d\n", TEST_STEPS[step].name, sent.r, sent.g, sent.b);
  }
  return TEST_STEPS[step].color;
}
