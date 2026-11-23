// test_pattern — steps all strands through red, green, blue, white, off.
// Non-blocking: call runTestPattern() every loop(); it changes color every STEP_MS.

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

void runTestPattern() {
  static int step = -1;
  static uint32_t lastStepMs = 0;
  uint32_t now = millis();

  if (step >= 0 && now - lastStepMs < STEP_MS) return;

  step = (step + 1) % NUM_TEST_STEPS;
  lastStepMs = now;

  for (int o = 0; o < NUM_OUTPUTS; o++) {
    fill_solid(frame[o], NUM_LEDS, TEST_STEPS[step].color);
  }
  showFrame();

  CRGB sent = out[0][0];
  Serial.printf("%-5s -> sent R%d G%d B%d\n", TEST_STEPS[step].name, sent.r, sent.g, sent.b);
}
