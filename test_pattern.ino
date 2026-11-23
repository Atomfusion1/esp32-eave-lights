// test_pattern — steps all strands through red, green, blue, white, off.
// Non-blocking: call runTestPattern() every loop(); it changes color every STEP_MS.

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
