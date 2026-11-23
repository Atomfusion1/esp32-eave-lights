// web_api — JSON API on port 80 (the page itself is in web_page.ino).
//
//   GET  /              control page
//   GET  /api/state     {"version", "storage", "outputs":[A,B], "house":{...},
//                        "zones":[{"id":"z0","name","ranges","length",...}],
//                        "paths":[{"id":"p0","name","summary","steps":[...],"length",...}],
//                        "locate":{...}}
//   POST /api/state     form-encoded body or URL query, any of:
//                         output=all|a|b|house|z<N>|p<N>  what to change (default all =
//                                both strands; "all" also lines up their fades/movement).
//                                house = Whole house path, z<N> = zone slot N,
//                                p<N> = zone path slot N.
//                         on=0|1  color=RRGGBB  brightness=0-255
//                         mode=solid|fade|test|chase|comet
//                         fadeSeconds=2-60  fadeMin=0-255
//                         speed=1-100 (lights/s)  size=1-30  reverse=0|1
//                         bgOn=0|1  bgColor=RRGGBB   (chase/comet background)
//                       Invalid values change nothing and return 400. Returns the new state.
//   GET  /api/frame     exact colors being sent right now (after power caps), binary:
//                       [outputs], per output [count low][count high], then R,G,B per
//                       light, A's lights first.
//   POST /api/strands   a=67 b=65   strand lengths (1-300), saved. Returns the state.
//   POST /api/all-off   turns off every strand, Whole house, zone and path. Returns the state.
//   POST /api/firmware  multipart .bin + password field: wireless firmware update (ota.ino)
//   POST /api/locate    ranges=A23 | A1-12,B30-65   light only those (auto-off after 5 min)
//                       [seconds=1-300] shorter auto-off, [white=1] all white (no green first)
//                       stop=1                      back to normal. Returns the state.
//   POST /api/zones     action=save   [id=N] name=Porch ranges=A1-12   (no id = new zone)
//                       action=delete id=N. Returns the state.
//   POST /api/paths     action=save   [id=N] name=Front sweep steps=0:0:0,2:1:3
//                         steps = zone slot : reverse 0|1 : gap lights after (0-30), in order
//                       action=delete id=N. Returns the state.
//   POST /api/scenes    action=save name=Xmas | action=apply id=N | action=delete id=N
//   GET  /api/files, /fs/<path>, POST /api/restore-*   backup and restore (backup.ino)
//   GET  /api/presets   {"presets":[{"id","name","mode","color","bgOn","bgColor"}],
//                        "scenes":[{"id","name","on","colors"}]}
//   POST /api/presets   action=save   name=Xmas output=<as above>   save its settings
//                       action=apply  id=N output=<as above>         returns the state
//                       action=delete id=N
//
// No login: anyone on the same network can use it. Fine on a home network.

const char* MODE_NAMES[MODE_COUNT] = { "solid", "fade", "test", "chase", "comet" };

// ---------- JSON ----------

// The settings fields shared by every channel, without braces.
String channelFieldsJson(int ch) {
  const LightState& s = states[ch];
  char buf[300];
  snprintf(buf, sizeof(buf),
           "\"on\":%s,\"color\":\"#%02X%02X%02X\",\"brightness\":%u,\"mode\":\"%s\","
           "\"fadeSeconds\":%u,\"fadeMin\":%u,\"speed\":%u,\"size\":%u,\"reverse\":%s,"
           "\"bgOn\":%s,\"bgColor\":\"#%02X%02X%02X\"",
           s.on ? "true" : "false", s.color.r, s.color.g, s.color.b, s.brightness,
           MODE_NAMES[s.mode], s.fadeSeconds, s.fadeMin, s.speed, s.size,
           s.reverse ? "true" : "false",
           s.bgOn ? "true" : "false", s.bgColor.r, s.bgColor.g, s.bgColor.b);
  return String(buf);
}

String stateJson() {
  String json = "{\"version\":\"" FW_VERSION "\",\"storage\":";
  json += storageOk ? "true" : "false";
  json += ",\"outputs\":[";
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    char buf[80];
    snprintf(buf, sizeof(buf), "%s{\"id\":\"%c\",\"gpio\":%d,\"enabled\":%s,\"count\":%u,",
             o ? "," : "", OUTPUT_LETTERS[o], OUTPUT_PINS[o],
             OUTPUT_ENABLED[o] ? "true" : "false", ledCount[o]);
    json += buf;
    json += channelFieldsJson(o);
    json += "}";
  }
  json += "],\"house\":{\"id\":\"house\",\"name\":\"Whole house\",\"length\":";
  json += houseLen;
  json += ",";
  json += channelFieldsJson(CH_HOUSE);
  json += "},\"zones\":[";
  bool first = true;
  for (int z = 0; z < MAX_ZONES; z++) {
    if (!zones[z].used) continue;
    if (!first) json += ",";
    first = false;
    json += "{\"id\":\"z";
    json += z;
    json += "\",\"slot\":";
    json += z;
    json += ",\"name\":\"";
    json += zones[z].name;
    json += "\",\"ranges\":\"";
    json += rangesText(zones[z].ranges, zones[z].rangeCount);
    json += "\",\"length\":";
    json += zoneLen[z];
    json += ",";
    json += channelFieldsJson(CH_ZONE0 + z);
    json += "}";
  }
  json += "],\"paths\":[";
  first = true;
  for (int p = 0; p < MAX_PATHS; p++) {
    if (!pathDefs[p].used) continue;
    if (!first) json += ",";
    first = false;
    json += "{\"id\":\"p";
    json += p;
    json += "\",\"slot\":";
    json += p;
    json += ",\"name\":\"";
    json += pathDefs[p].name;
    json += "\",\"summary\":\"";
    json += pathStepsText(p);
    json += "\",\"steps\":[";
    for (int s = 0; s < pathDefs[p].stepCount; s++) {
      const PathStep& st = pathDefs[p].steps[s];
      char buf[48];
      snprintf(buf, sizeof(buf), "%s{\"zone\":%u,\"reverse\":%s,\"gap\":%u}",
               s ? "," : "", st.zone, st.reverse ? "true" : "false", st.gapAfter);
      json += buf;
    }
    json += "],\"length\":";
    json += pathLen[p];
    json += ",";
    json += channelFieldsJson(CH_PATH0 + p);
    json += "}";
  }
  json += "],\"locate\":{\"active\":";
  json += locateActive ? "true" : "false";
  json += ",\"ranges\":\"";
  if (locateActive) json += rangesText(locateRanges, locateCount);
  json += "\"}}";
  return json;
}

// Saved looks and scenes: {"presets":[...],"scenes":[...]}
String presetsJson() {
  String json = "{\"presets\":[";
  bool first = true;
  for (int p = 0; p < MAX_PRESETS; p++) {
    if (!presets[p].used) continue;
    const LightState& s = presets[p].look;
    char buf[200];
    snprintf(buf, sizeof(buf),
             "%s{\"id\":%d,\"name\":\"%s\",\"mode\":\"%s\",\"color\":\"#%02X%02X%02X\","
             "\"bgOn\":%s,\"bgColor\":\"#%02X%02X%02X\"}",
             first ? "" : ",", p, presets[p].name, MODE_NAMES[s.mode],
             s.color.r, s.color.g, s.color.b, s.bgOn ? "true" : "false",
             s.bgColor.r, s.bgColor.g, s.bgColor.b);
    json += buf;
    first = false;
  }
  json += "],\"scenes\":";
  json += scenesJson();
  json += "}";
  return json;
}

void sendError(const char* message) {
  String body = "{\"error\":\"";
  body += message;
  body += "\"}";
  server.send(400, "application/json", body);
}

void sendState() {
  server.send(200, "application/json", stateJson());
}

// ---------- Argument helpers ----------

// Reads an "RRGGBB" or "#RRGGBB" argument. Returns false if it isn't 6 hex digits.
bool parseColorArg(const char* name, CRGB& result) {
  String v = server.arg(name);
  if (v.startsWith("#")) v = v.substring(1);
  if (v.length() != 6) return false;
  for (unsigned int i = 0; i < 6; i++) {
    if (!isxdigit((unsigned char)v[i])) return false;
  }
  result = CRGB((uint32_t)strtoul(v.c_str(), nullptr, 16));
  return true;
}

// Reads the "output" argument: -1 = both strands, else a channel number.
// Returns false if it names nothing.
bool parseTarget(int& target) {
  target = -1;
  if (!server.hasArg("output")) return true;
  String v = server.arg("output");
  v.toLowerCase();
  if (v == "all")                        { target = -1; return true; }
  if (v == "a" || v == "0")              { target = 0; return true; }
  if (v == "b" || v == "1")              { target = 1; return true; }
  if (v == "house" || v == "path")       { target = CH_HOUSE; return true; }
  if (v.length() >= 2 && v[0] == 'z') {
    int z = v.substring(1).toInt();
    if (z >= 0 && z < MAX_ZONES && zones[z].used) { target = CH_ZONE0 + z; return true; }
  }
  if (v.length() >= 2 && v[0] == 'p') {
    int p = v.substring(1).toInt();
    if (p >= 0 && p < MAX_PATHS && pathDefs[p].used) { target = CH_PATH0 + p; return true; }
  }
  return false;
}

bool argIsTrue(const char* name) {
  String v = server.arg(name);
  return v == "1" || v == "true" || v == "on";
}

// ---------- Handlers ----------

void handleSetState() {
  // ---- 1. Parse and validate everything first, so a bad value changes nothing ----
  int target;
  if (!parseTarget(target)) return sendError("output must be all, a, b, house or a zone");

  bool hasColor = server.hasArg("color");
  CRGB newColor;
  if (hasColor && !parseColorArg("color", newColor)) return sendError("color must be RRGGBB");

  bool hasBgColor = server.hasArg("bgColor");
  CRGB newBgColor;
  if (hasBgColor && !parseColorArg("bgColor", newBgColor)) return sendError("bgColor must be RRGGBB");

  bool hasMode = server.hasArg("mode");
  Mode newMode = MODE_SOLID;
  if (hasMode) {
    String v = server.arg("mode");
    int found = -1;
    for (int i = 0; i < MODE_COUNT; i++) {
      if (v == MODE_NAMES[i]) found = i;
    }
    if (found < 0) return sendError("mode must be solid, fade, test, chase or comet");
    newMode = (Mode)found;
  }

  // ---- 2. Apply to the target channel(s) ----
  bool changed = false;
  for (int o = 0; o < NUM_CHANNELS; o++) {
    if (target < 0 && o >= NUM_OUTPUTS) continue;   // "all" = the strands only
    if (target >= 0 && o != target) continue;
    LightState& s = states[o];

    if (server.hasArg("on")) {
      bool wasOn = s.on;
      s.on = argIsTrue("on");
      if (o >= CH_HOUSE && s.on && !wasOn) resetPhase(o);  // paths/zones start from the top
      changed = true;
    }
    if (hasColor) { s.color = newColor; changed = true; }
    if (server.hasArg("brightness")) {
      s.brightness = (uint8_t)constrain(server.arg("brightness").toInt(), 0, 255);
      changed = true;
    }
    if (hasMode) { s.mode = newMode; changed = true; }
    if (server.hasArg("fadeSeconds")) {
      s.fadeSeconds = (uint8_t)constrain(server.arg("fadeSeconds").toInt(),
                                         FADE_SECONDS_MIN, FADE_SECONDS_MAX);
      changed = true;
    }
    if (server.hasArg("fadeMin")) {
      s.fadeMin = (uint8_t)constrain(server.arg("fadeMin").toInt(), 0, 255);
      changed = true;
    }
    if (server.hasArg("speed")) {
      s.speed = (uint8_t)constrain(server.arg("speed").toInt(), SPEED_MIN, SPEED_MAX);
      changed = true;
    }
    if (server.hasArg("size")) {
      s.size = (uint8_t)constrain(server.arg("size").toInt(), SIZE_MIN, SIZE_MAX);
      changed = true;
    }
    if (server.hasArg("reverse")) { s.reverse = argIsTrue("reverse"); changed = true; }
    if (server.hasArg("bgOn"))    { s.bgOn = argIsTrue("bgOn"); changed = true; }
    if (hasBgColor) { s.bgColor = newBgColor; changed = true; }
  }

  // Changing both strands together: keep their fades and movement in step.
  if (target < 0 && (hasMode || server.hasArg("on") || server.hasArg("fadeSeconds") ||
                     server.hasArg("speed") || server.hasArg("size") || server.hasArg("reverse"))) {
    syncPhases();
  }

  if (changed) markStateChanged();
  sendState();
}

// Live view: copies out[] (what FastLED is sending, after caps) into one binary reply:
//   [outputs] then per output [count low][count high], then R,G,B per light,
//   all of A's lights first, then B's.
// out[] holds plain RGB; the RBG reorder for the strand happens later inside FastLED.
void handleGetFrame() {
  static uint8_t buf[1 + NUM_OUTPUTS * 2 + MAX_TOTAL_LEDS * 3];
  size_t n = 0;
  buf[n++] = NUM_OUTPUTS;
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    buf[n++] = ledCount[o] & 0xFF;
    buf[n++] = ledCount[o] >> 8;
  }
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    for (int i = 0; i < ledCount[o]; i++) {
      buf[n++] = out[o][i].r;
      buf[n++] = out[o][i].g;
      buf[n++] = out[o][i].b;
    }
  }
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "application/octet-stream", (const char*)buf, n);
}

// Turns off everything: strands, Whole house, every zone and path (and Find a light).
void handleAllOff() {
  stopLocate();
  for (int ch = 0; ch < NUM_CHANNELS; ch++) states[ch].on = false;
  markStateChanged();
  sendState();
}

// Sets strand lengths: a=<lights on A> b=<lights on B> (1-MAX_LEDS each).
void handleStrands() {
  int a = server.hasArg("a") ? server.arg("a").toInt() : ledCount[0];
  int b = server.hasArg("b") ? server.arg("b").toInt() : ledCount[1];
  if (a < 1 || a > MAX_LEDS || b < 1 || b > MAX_LEDS) return sendError("each strand needs 1 to 300 lights");
  stopLocate();
  setLedCounts(a, b);
  sendState();
}

void handleLocate() {
  if (server.hasArg("stop") || server.arg("ranges").length() == 0) {
    stopLocate();
    return sendState();
  }
  ZoneRange ranges[MAX_ZONE_RANGES];
  uint8_t count;
  const char* err = parseRanges(server.arg("ranges"), ranges, count);
  if (err) return sendError(err);
  // seconds=1-300: how long to show (default 5 minutes); white=1: every light white
  uint32_t timeoutMs = LOCATE_TIMEOUT_MS;
  if (server.hasArg("seconds")) timeoutMs = (uint32_t)constrain(server.arg("seconds").toInt(), 1, 300) * 1000;
  startLocate(ranges, count, timeoutMs, argIsTrue("white"));
  sendState();
}

void handleZones() {
  String action = server.arg("action");
  int id = server.hasArg("id") && server.arg("id").length() ? server.arg("id").toInt() : -1;
  const char* err = nullptr;
  if (action == "save") {
    int newId;
    err = saveZone(id, server.arg("name"), server.arg("ranges"), newId);
  } else if (action == "delete") {
    err = deleteZone(id);
  } else {
    err = "action must be save or delete";
  }
  if (err) return sendError(err);
  sendState();
}

void handlePaths() {
  String action = server.arg("action");
  int id = server.hasArg("id") && server.arg("id").length() ? server.arg("id").toInt() : -1;
  const char* err = nullptr;
  if (action == "save") {
    int newId;
    err = saveZonePath(id, server.arg("name"), server.arg("steps"), newId);
  } else if (action == "delete") {
    err = deleteZonePath(id);
  } else {
    err = "action must be save or delete";
  }
  if (err) return sendError(err);
  sendState();
}

void handleGetPresets() {
  server.send(200, "application/json", presetsJson());
}

void handlePresets() {
  String action = server.arg("action");
  int id = server.arg("id").length() ? server.arg("id").toInt() : -1;
  int target;
  if (!parseTarget(target)) return sendError("output must be all, a, b, house or a zone");

  if (action == "save") {
    int newId;
    const char* err = savePreset(server.arg("name"), target < 0 ? 0 : target, newId);
    if (err) return sendError(err);
    return handleGetPresets();
  }
  if (action == "delete") {
    const char* err = deletePreset(id);
    if (err) return sendError(err);
    return handleGetPresets();
  }
  if (action == "apply") {
    const char* err = nullptr;
    if (target < 0) {
      for (int o = 0; o < NUM_OUTPUTS && !err; o++) err = applyPreset(id, o);
      syncPhases();
    } else {
      err = applyPreset(id, target);
    }
    if (err) return sendError(err);
    return sendState();
  }
  sendError("action must be save, apply or delete");
}

// Scenes: action=save name=Xmas | action=apply id=N | action=delete id=N.
// save/delete return the looks+scenes list; apply returns the state.
void handleScenes() {
  String action = server.arg("action");
  int id = server.arg("id").length() ? server.arg("id").toInt() : -1;
  const char* err;
  if (action == "save")        err = saveScene(server.arg("name"));
  else if (action == "delete") err = deleteScene(id);
  else if (action == "apply") {
    err = applyScene(id);
    if (err) return sendError(err);
    return sendState();
  } else {
    err = "action must be save, apply or delete";
  }
  if (err) return sendError(err);
  handleGetPresets();
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void startWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/state", HTTP_POST, handleSetState);
  server.on("/api/frame", HTTP_GET, handleGetFrame);
  server.on("/api/locate", HTTP_POST, handleLocate);
  server.on("/api/strands", HTTP_POST, handleStrands);
  server.on("/api/all-off", HTTP_POST, handleAllOff);
  server.on("/api/firmware", HTTP_POST, handleFirmwareDone, handleFirmwareData);
  server.on("/api/zones", HTTP_POST, handleZones);
  server.on("/api/paths", HTTP_POST, handlePaths);
  server.on("/api/presets", HTTP_GET, handleGetPresets);
  server.on("/api/presets", HTTP_POST, handlePresets);
  addFileRoutes();   // House view editor, photos, layout (files.ino)
  addBackupRoutes(); // backup / restore (backup.ino)
  server.on("/api/scenes", HTTP_POST, handleScenes);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  Serial.println("Web: server started on port 80");
}

void handleWeb() {
  server.handleClient();
}
