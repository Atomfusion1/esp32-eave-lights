// schedule - playlists of scenes and the yearly calendar.
//
// A playlist (Program) is a list of scenes, each shown for `seconds`, looping. With one
// scene it simply holds that scene.
//
// What plays:
//   1. a playlist started with Play (manualProgram), until Stop
//   2. otherwise, if the calendar is on and the clock is set: the playlist whose yearly
//      date range includes today (the shortest range wins, so "Dec 20-26" beats "Dec 1-31")
//   3. otherwise nothing: the lights stay as they are
// Stop also pauses the calendar for the rest of the day.
//
// The clock comes from the internet (NTP, TIMEZONE in the main tab) once Wi-Fi connects.
// Scene swaps are not saved to flash (applyScene(..., false)); after a power cut the
// calendar or the saved manual playlist picks up again.
//
// Saved: playlists in /programs.bin; calendar on/off and the manual playlist in
// Preferences ("sched", "mprog", see settings_store.ino).

const char*    PROGRAMS_FILE    = "/programs.bin";
const uint32_t PROGRAMS_MAGIC   = 0x474F5250;  // "PROG"
const uint16_t PROGRAMS_VERSION = 1;

bool     clockStarted  = false;
int      activeProgram = -1;     // playlist playing now, or -1
int      activeStep    = 0;
uint32_t stepStartMs   = 0;
int      stoppedYday   = -1;     // day of the year Stop was pressed (calendar paused)
bool     scheduleForce = false;  // re-check right away (after Play/Stop/edits)

// True (and fills t) once the internet clock has been set.
bool clockNow(struct tm& t) {
  if (!getLocalTime(&t, 0)) return false;
  return t.tm_year + 1900 >= 2025;
}

// Day number 1-365 (no leap day) for comparing yearly dates.
int yearDay(int month, int day) {
  static const int before[12] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  if (month < 1 || month > 12) return 1;
  return before[month - 1] + constrain(day, 1, 31);
}

bool programHasDate(int p, int month, int day) {
  const Program& pr = programs[p];
  int k = yearDay(month, day), a = yearDay(pr.fromMonth, pr.fromDay), b = yearDay(pr.toMonth, pr.toDay);
  return a <= b ? (k >= a && k <= b) : (k >= a || k <= b);   // a > b: wraps over New Year
}

int programRangeDays(int p) {
  const Program& pr = programs[p];
  int a = yearDay(pr.fromMonth, pr.fromDay), b = yearDay(pr.toMonth, pr.toDay);
  return b >= a ? b - a + 1 : 365 - a + b + 1;
}

// The playlist for this date, or -1.
int programForDate(int month, int day) {
  int best = -1, bestDays = 999;
  for (int p = 0; p < MAX_PROGRAMS; p++) {
    if (!programs[p].used || !programs[p].useDates || !programs[p].stepCount) continue;
    if (!programHasDate(p, month, day)) continue;
    int days = programRangeDays(p);
    if (days < bestDays) { best = p; bestDays = days; }
  }
  return best;
}

// Shows the active step's scene (skipping steps whose scene was deleted).
void startProgramStep() {
  const Program& pr = programs[activeProgram];
  for (int tries = 0; tries < pr.stepCount; tries++) {
    int sc = pr.steps[activeStep].scene;
    if (sc < MAX_SCENES && scenes[sc].used) {
      applyScene(sc, false);
      stepStartMs = millis();
      Serial.printf("Playlist %s: step %d\n", pr.name, activeStep + 1);
      return;
    }
    activeStep = (activeStep + 1) % pr.stepCount;
  }
  Serial.printf("Playlist %s: none of its scenes exist any more\n", pr.name);
  activeProgram = -1;
}

// ---------- On/off timer ----------
// At the on time: "All lights on (as before)". At the off time: "All lights off".
// It acts only when the time is crossed (and once after start-up to catch up), so lights
// switched by hand in between stay as they are. Playlists pause while it's "off".

int  timerLastWindow = -1;   // -1 = not checked yet, 0 = off window, 1 = on window
bool timerHoldsOff   = false;

// Minutes the clock is ahead of UTC right now (Eastern: -300 or -240).
int utcOffsetMinutes() {
  time_t now = time(nullptr);
  struct tm lt, gt;
  localtime_r(&now, &lt);
  gmtime_r(&now, &gt);
  int d = (lt.tm_hour * 60 + lt.tm_min) - (gt.tm_hour * 60 + gt.tm_min);
  if (lt.tm_yday != gt.tm_yday) d += (lt.tm_yday > gt.tm_yday || (lt.tm_yday == 0 && gt.tm_yday > 300)) ? 1440 : -1440;
  return d;
}

// Sunrise or sunset for day-of-year yday (0-365) in local minutes after midnight,
// or -1 if the sun doesn't rise/set (never in Ohio). Standard almanac formula, +/- a minute.
int sunMinutes(int yday, bool sunrise) {
  const float D = PI / 180.0f;
  float lngHour = lightTimer.lon / 15.0f;
  float t = (yday + 1) + ((sunrise ? 6.0f : 18.0f) - lngHour) / 24.0f;
  float M = 0.9856f * t - 3.289f;
  float L = M + 1.916f * sinf(M * D) + 0.020f * sinf(2 * M * D) + 282.634f;
  L = fmodf(L + 720.0f, 360.0f);
  float RA = atanf(0.91764f * tanf(L * D)) / D;
  RA = fmodf(RA + 720.0f, 360.0f);
  RA += floorf(L / 90.0f) * 90.0f - floorf(RA / 90.0f) * 90.0f;
  RA /= 15.0f;
  float sinDec = 0.39782f * sinf(L * D), cosDec = cosf(asinf(sinDec));
  float cosH = (cosf(90.833f * D) - sinDec * sinf(lightTimer.lat * D)) / (cosDec * cosf(lightTimer.lat * D));
  if (cosH > 1 || cosH < -1) return -1;
  float H = (sunrise ? 360.0f - acosf(cosH) / D : acosf(cosH) / D) / 15.0f;
  float T = H + RA - 0.06571f * t - 6.622f;
  float UT = fmodf(T - lngHour + 48.0f, 24.0f);
  int m = (int)lroundf(UT * 60.0f) + utcOffsetMinutes();
  return ((m % 1440) + 1440) % 1440;
}

// Today's on and off times in minutes after midnight (-1 = never).
void timerTimes(const struct tm& t, int& onMin, int& offMin) {
  onMin = lightTimer.onMode == TIMER_SUN ? sunMinutes(t.tm_yday, false) + lightTimer.onOffset : lightTimer.onClock;
  if (lightTimer.offMode == TIMER_NEVER) offMin = -1;
  else offMin = lightTimer.offMode == TIMER_SUN ? sunMinutes(t.tm_yday, true) + lightTimer.offOffset : lightTimer.offClock;
  onMin = ((onMin % 1440) + 1440) % 1440;
  if (offMin >= 0) offMin = ((offMin % 1440) + 1440) % 1440;
}

// 1 = lights should be on now, 0 = off.
int timerWindow(const struct tm& t) {
  int onMin, offMin;
  timerTimes(t, onMin, offMin);
  int now = t.tm_hour * 60 + t.tm_min;
  if (offMin < 0) return now >= onMin ? 1 : 0;           // never off: on from the on time
  if (onMin < offMin) return (now >= onMin && now < offMin) ? 1 : 0;
  return (now >= onMin || now < offMin) ? 1 : 0;         // e.g. on 17:00, off 07:00
}

void handleTimer(bool haveClock, const struct tm& t) {
  if (!lightTimer.enabled || !haveClock) { timerHoldsOff = false; timerLastWindow = -1; return; }
  int w = timerWindow(t);
  if (w == timerLastWindow) return;
  bool first = timerLastWindow < 0;
  timerLastWindow = w;
  if (w) {
    timerHoldsOff = false;
    // after start-up only switch on if nothing is on yet (don't undo a playlist's scene)
    bool anyOn = false;
    for (int ch = 0; ch < NUM_CHANNELS; ch++) if (states[ch].on && channelInUse(ch)) anyOn = true;
    if (!first || !anyOn) allLightsOn();
    activeProgram = -1;                                   // let the calendar/playlist pick again
    Serial.println("Timer: lights on");
  } else {
    timerHoldsOff = true;
    allLightsOff();
    activeProgram = -1;
    Serial.println("Timer: lights off");
  }
}

void handleSchedule() {
  static uint32_t lastCheck = 0;
  if (!scheduleForce && millis() - lastCheck < 1000) return;
  lastCheck = millis();
  scheduleForce = false;

  if (!clockStarted && WiFi.status() == WL_CONNECTED) {
    configTzTime(TIMEZONE, "pool.ntp.org", "time.nist.gov");
    clockStarted = true;
  }

  struct tm t;
  bool haveClock = clockNow(t);
  handleTimer(haveClock, t);
  int want = -1;
  if (timerHoldsOff) {
    want = -1;                                           // timer has the lights off
  } else if (manualProgram >= 0 && manualProgram < MAX_PROGRAMS && programs[manualProgram].used) {
    want = manualProgram;
  } else if (scheduleOn && haveClock && t.tm_yday != stoppedYday) {
    want = programForDate(t.tm_mon + 1, t.tm_mday);
  }

  if (want != activeProgram) {
    activeProgram = want;
    activeStep = 0;
    if (want >= 0 && programs[want].stepCount) startProgramStep();
    else { activeProgram = -1; Serial.println("Playlist: none playing"); }
    return;
  }
  if (activeProgram < 0) return;

  const Program& pr = programs[activeProgram];
  if (pr.stepCount <= 1) return;                                // one scene: just hold it
  if (activeStep >= pr.stepCount) activeStep = 0;
  if (millis() - stepStartMs >= pr.steps[activeStep].seconds * 1000UL) {
    activeStep = (activeStep + 1) % pr.stepCount;
    startProgramStep();
  }
}

// ---- Controls (from the page) ----

// Makes handleSchedule() re-check on its next call (after a restore etc.).
void nudgeSchedule() {
  activeProgram = -1;
  scheduleForce = true;
}

void playProgram(int id) {
  manualProgram = id;
  stoppedYday = -1;
  activeProgram = -1;          // restart it from the first scene
  scheduleForce = true;
  markStateChanged();          // saves manualProgram
}

void stopProgram() {
  manualProgram = -1;
  struct tm t;
  if (clockNow(t)) stoppedYday = t.tm_yday;   // calendar waits until tomorrow
  activeProgram = -1;
  markStateChanged();
}

void nextProgramStep() {
  if (activeProgram < 0) return;
  activeStep = (activeStep + 1) % max((int)programs[activeProgram].stepCount, 1);
  startProgramStep();
}

// From the page: enabled=0|1 onMode=0|1 onClock onOffset offMode=0|1|2 offClock offOffset lat lon
void setTimerFromArgs() {
  LightTimer& lt = lightTimer;
  if (server.hasArg("enabled"))   lt.enabled   = argIsTrue("enabled");
  if (server.hasArg("onMode"))    lt.onMode    = server.arg("onMode").toInt() == 1 ? TIMER_CLOCK : TIMER_SUN;
  if (server.hasArg("onClock"))   lt.onClock   = constrain(server.arg("onClock").toInt(), 0, 1439);
  if (server.hasArg("onOffset"))  lt.onOffset  = constrain(server.arg("onOffset").toInt(), -180, 180);
  if (server.hasArg("offMode")) {
    int m = server.arg("offMode").toInt();
    lt.offMode = m == 1 ? TIMER_CLOCK : m == 2 ? TIMER_NEVER : TIMER_SUN;
  }
  if (server.hasArg("offClock"))  lt.offClock  = constrain(server.arg("offClock").toInt(), 0, 1439);
  if (server.hasArg("offOffset")) lt.offOffset = constrain(server.arg("offOffset").toInt(), -180, 180);
  if (server.hasArg("lat"))       lt.lat       = constrain(server.arg("lat").toFloat(), -66.0f, 66.0f);
  if (server.hasArg("lon"))       lt.lon       = constrain(server.arg("lon").toFloat(), -180.0f, 180.0f);
  timerLastWindow = -1;          // re-check now (catches up if we're inside the on window)
  timerHoldsOff = false;
  scheduleForce = true;
  markStateChanged();
}

void setScheduleOn(bool on) {
  scheduleOn = on;
  if (on) stoppedYday = -1;
  scheduleForce = true;
  markStateChanged();
}

// "MM-DD" -> month, day. Returns false if not a real date.
bool parseMonthDay(const String& s, uint8_t& m, uint8_t& d) {
  int dash = s.indexOf('-');
  if (dash < 1) return false;
  int mm = s.substring(0, dash).toInt(), dd = s.substring(dash + 1).toInt();
  static const int days[12] = { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (mm < 1 || mm > 12 || dd < 1 || dd > days[mm - 1]) return false;
  m = mm; d = dd;
  return true;
}

// steps text: "scene:seconds,..." e.g. "0:600,2:300". id < 0 = new playlist.
const char* saveProgram(int id, const String& name, const String& stepsText,
                        bool useDates, const String& from, const String& to) {
  char cleanName[NAME_LEN];
  if (!copyName(name, cleanName)) return "enter a name for the playlist";

  Program pr = {};
  pr.used = true;
  strcpy(pr.name, cleanName);
  pr.useDates = useDates;
  if (useDates) {
    if (!parseMonthDay(from, pr.fromMonth, pr.fromDay) || !parseMonthDay(to, pr.toMonth, pr.toDay)) {
      return "pick valid dates";
    }
  } else {
    pr.fromMonth = 1; pr.fromDay = 1; pr.toMonth = 12; pr.toDay = 31;
  }

  int pos = 0, len = stepsText.length();
  while (pos < len) {
    int comma = stepsText.indexOf(',', pos);
    if (comma < 0) comma = len;
    String item = stepsText.substring(pos, comma);
    pos = comma + 1;
    item.trim();
    if (!item.length()) continue;
    if (pr.stepCount >= MAX_PROGRAM_STEPS) return "too many scenes (max 10)";
    int colon = item.indexOf(':');
    if (colon < 1) return "bad step";
    int sc = item.substring(0, colon).toInt();
    long secs = item.substring(colon + 1).toInt();
    if (sc < 0 || sc >= MAX_SCENES || !scenes[sc].used) return "a step uses a scene that doesn't exist";
    pr.steps[pr.stepCount++] = { (uint8_t)sc, (uint32_t)constrain(secs, 10L, 24L * 3600) };
  }
  if (!pr.stepCount) return "add at least one scene";

  if (id < 0) {
    for (int p = 0; p < MAX_PROGRAMS && id < 0; p++) if (!programs[p].used) id = p;
    if (id < 0) return "all 12 playlists are used; delete one first";
  } else if (id >= MAX_PROGRAMS || !programs[id].used) {
    return "playlist not found";
  }
  programs[id] = pr;
  if (id == activeProgram) activeProgram = -1;   // restart it with the new list
  scheduleForce = true;
  saveProgramsFile();
  return nullptr;
}

const char* deleteProgram(int id) {
  if (id < 0 || id >= MAX_PROGRAMS || !programs[id].used) return "playlist not found";
  programs[id].used = false;
  if (manualProgram == id) { manualProgram = -1; markStateChanged(); }
  if (activeProgram == id) activeProgram = -1;
  scheduleForce = true;
  saveProgramsFile();
  return nullptr;
}

// ---- JSON for the page ----

String programsJson() {
  struct tm t;
  bool haveClock = clockNow(t);
  String j = "{\"scheduleOn\":";
  j += scheduleOn ? "true" : "false";
  j += ",\"clock\":\"";
  if (haveClock) {
    char buf[40];
    strftime(buf, sizeof(buf), "%a %b %d, %H:%M", &t);
    j += buf;
  }
  j += "\",\"timer\":{\"enabled\":";
  j += lightTimer.enabled ? "true" : "false";
  j += ",\"onMode\":" + String(lightTimer.onMode) + ",\"onClock\":" + String(lightTimer.onClock) +
       ",\"onOffset\":" + String(lightTimer.onOffset) + ",\"offMode\":" + String(lightTimer.offMode) +
       ",\"offClock\":" + String(lightTimer.offClock) + ",\"offOffset\":" + String(lightTimer.offOffset) +
       ",\"lat\":" + String(lightTimer.lat, 3) + ",\"lon\":" + String(lightTimer.lon, 3);
  if (haveClock) {
    int onMin, offMin;
    timerTimes(t, onMin, offMin);
    j += ",\"todayOn\":" + String(onMin) + ",\"todayOff\":" + String(offMin) +
         ",\"sunset\":" + String(sunMinutes(t.tm_yday, false)) + ",\"sunrise\":" + String(sunMinutes(t.tm_yday, true)) +
         ",\"holdsOff\":" + (timerHoldsOff ? "true" : "false");
  }
  j += "}";
  j += ",\"stoppedToday\":";
  j += (haveClock && stoppedYday == t.tm_yday) ? "true" : "false";
  j += ",\"active\":";
  if (activeProgram >= 0) {
    const Program& pr = programs[activeProgram];
    long left = -1;
    if (pr.stepCount > 1) {
      uint32_t el = millis() - stepStartMs, dur = pr.steps[activeStep].seconds * 1000UL;
      left = el >= dur ? 0 : (long)((dur - el) / 1000);
    }
    j += "{\"id\":" + String(activeProgram) + ",\"step\":" + String(activeStep) +
         ",\"manual\":" + (manualProgram == activeProgram ? "true" : "false") +
         ",\"nextIn\":" + String(left) + "}";
  } else {
    j += "null";
  }
  j += ",\"programs\":[";
  bool first = true;
  for (int p = 0; p < MAX_PROGRAMS; p++) {
    const Program& pr = programs[p];
    if (!pr.used) continue;
    if (!first) j += ",";
    first = false;
    char head[160];
    snprintf(head, sizeof(head), "{\"id\":%d,\"name\":\"%s\",\"useDates\":%s,\"from\":\"%02u-%02u\",\"to\":\"%02u-%02u\",\"steps\":[",
             p, pr.name, pr.useDates ? "true" : "false", pr.fromMonth, pr.fromDay, pr.toMonth, pr.toDay);
    j += head;
    for (int s = 0; s < pr.stepCount; s++) {
      j += String(s ? "," : "") + "{\"scene\":" + String(pr.steps[s].scene) + ",\"seconds\":" + String(pr.steps[s].seconds) + "}";
    }
    j += "]}";
  }
  j += "]}";
  return j;
}

// ---- File ----

void loadPrograms() {
  for (int p = 0; p < MAX_PROGRAMS; p++) programs[p].used = false;
  if (!storageOk || !FFat.exists(PROGRAMS_FILE)) return;
  File f = FFat.open(PROGRAMS_FILE, "r");
  if (!f) return;
  FileHeader h;
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h) || h.magic != PROGRAMS_MAGIC ||
      h.version != PROGRAMS_VERSION || h.recordSize != sizeof(Program)) {
    Serial.println("Playlists: file is from a different firmware layout, starting empty");
    f.close();
    return;
  }
  int loaded = 0;
  for (int p = 0; p < h.count && p < MAX_PROGRAMS; p++) {
    if (f.read((uint8_t*)&programs[p], sizeof(Program)) != sizeof(Program)) break;
    programs[p].name[NAME_LEN - 1] = 0;
    if (programs[p].stepCount > MAX_PROGRAM_STEPS) programs[p].used = false;
    if (programs[p].used) loaded++;
  }
  f.close();
  Serial.printf("Playlists: %d loaded, calendar %s\n", loaded, scheduleOn ? "on" : "off");
}

void saveProgramsFile() {
  if (!storageOk || savesBlocked) return;
  File f = FFat.open(PROGRAMS_FILE, "w");
  if (!f) {
    Serial.println("Playlists: could not write file");
    return;
  }
  FileHeader h = { PROGRAMS_MAGIC, PROGRAMS_VERSION, (uint16_t)sizeof(Program), MAX_PROGRAMS };
  f.write((const uint8_t*)&h, sizeof(h));
  for (int p = 0; p < MAX_PROGRAMS; p++) f.write((const uint8_t*)&programs[p], sizeof(Program));
  f.close();
}
