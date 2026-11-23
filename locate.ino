// locate — "Find a light": shows only the chosen lights so you can see where they are
// on the house. Normally the first light of each range is green (shows the direction)
// and the rest white; "all white" mode lights every chosen light white (used when
// clicking a zone in the zone list).
// Turns itself off after its timeout (default LOCATE_TIMEOUT_MS) so it can't be left
// on by accident.

const uint32_t LOCATE_TIMEOUT_MS  = 5UL * 60 * 1000;
const CRGB     LOCATE_FIRST_COLOR = CRGB(0, 255, 0);
const CRGB     LOCATE_COLOR       = CRGB(255, 255, 255);

uint32_t locateTimeoutMs = LOCATE_TIMEOUT_MS;
bool     locateAllWhite  = false;

// Starts showing `count` ranges (copied) for timeoutMs, or stops if count is 0.
void startLocate(const ZoneRange* ranges, uint8_t count, uint32_t timeoutMs, bool allWhite) {
  locateCount = min((int)count, MAX_ZONE_RANGES);
  for (int r = 0; r < locateCount; r++) locateRanges[r] = ranges[r];
  locateActive = locateCount > 0;
  locateStartMs = millis();
  locateTimeoutMs = timeoutMs;
  locateAllWhite = allWhite;
}

void stopLocate() {
  locateActive = false;
  locateCount = 0;
}

void handleLocateTimeout() {
  if (locateActive && millis() - locateStartMs > locateTimeoutMs) {
    stopLocate();
    Serial.println("Find a light: timed out, back to normal");
  }
}

void drawLocate() {
  for (int o = 0; o < NUM_OUTPUTS; o++) fill_solid(frame[o], MAX_LEDS, CRGB(0, 0, 0));
  for (int r = 0; r < locateCount; r++) {
    const ZoneRange& z = locateRanges[r];
    int step = (z.first <= z.last) ? 1 : -1;
    for (int i = z.first; ; i += step) {
      bool firstGreen = !locateAllWhite && i == z.first;
      if (i < ledCount[z.output]) frame[z.output][i] = firstGreen ? LOCATE_FIRST_COLOR : LOCATE_COLOR;
      if (i == z.last) break;
    }
  }
}
