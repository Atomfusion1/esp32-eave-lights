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
  int want = -1;
  if (manualProgram >= 0 && manualProgram < MAX_PROGRAMS && programs[manualProgram].used) {
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
  j += "\",\"stoppedToday\":";
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
