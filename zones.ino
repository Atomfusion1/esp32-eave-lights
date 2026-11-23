// zones â€” named light ranges with their own settings, saved to /zones.bin.
//
// Ranges are written like the page shows them: "A1-12, B30-65", "A12-1" (backwards),
// "B5" (one light). Numbers are 1-based light numbers; stored 0-based.
//
// /zones.bin: header (magic, version, record size, count), then per zone slot:
// ZoneDef followed by that zone's LightState. If the record size doesn't match (the
// structs changed in a firmware update) the file is ignored and zones start empty.

const char*    ZONES_FILE    = "/zones.bin";
const uint32_t ZONES_MAGIC   = 0x5A4E4F5A;  // "ZONZ"
const uint16_t ZONES_VERSION = 1;

void initZones() {
  for (int z = 0; z < MAX_ZONES; z++) {
    zones[z].used = false;
    zones[z].name[0] = 0;
    zones[z].rangeCount = 0;
    zoneLen[z] = 0;
    states[CH_ZONE0 + z] = ZONE_DEFAULT_STATE;
  }
}

// Rebuilds zone z's light order from its ranges.
void rebuildZone(int z) {
  uint16_t n = 0;
  if (zones[z].used) {
    for (int r = 0; r < zones[z].rangeCount; r++) {
      const ZoneRange& zr = zones[z].ranges[r];
      int step = (zr.first <= zr.last) ? 1 : -1;
      for (int i = zr.first; n < HOUSE_PATH_LEN; i += step) {
        zonePoints[z][n++] = { zr.output, (uint16_t)i };
        if (i == zr.last) break;
      }
    }
  }
  zoneLen[z] = n;
}

// ---- Range text ----

// Parses "A1-12, B30-65, A12-1, B5" into ranges. Also accepts "A1-A12".
// Returns an error message, or nullptr if OK.
const char* parseRanges(const String& text, ZoneRange* outRanges, uint8_t& count) {
  count = 0;
  int pos = 0, len = text.length();
  while (pos < len) {
    // skip separators
    while (pos < len && (text[pos] == ' ' || text[pos] == ',')) pos++;
    if (pos >= len) break;
    if (count >= MAX_ZONE_RANGES) return "too many ranges (max 6)";

    char strand = toupper(text[pos++]);
    int out = -1;
    for (int o = 0; o < NUM_OUTPUTS; o++) if (OUTPUT_LETTERS[o] == strand) out = o;
    if (out < 0) return "each range must start with A or B, e.g. A1-12";

    int first = 0, digits = 0;
    while (pos < len && isdigit((unsigned char)text[pos])) { first = first * 10 + (text[pos++] - '0'); digits++; }
    if (!digits) return "missing light number, e.g. A1-12";

    int last = first;
    while (pos < len && text[pos] == ' ') pos++;
    if (pos < len && text[pos] == '-') {
      pos++;
      while (pos < len && text[pos] == ' ') pos++;
      if (pos < len && toupper(text[pos]) == strand) pos++;   // allow "A1-A12"
      last = 0; digits = 0;
      while (pos < len && isdigit((unsigned char)text[pos])) { last = last * 10 + (text[pos++] - '0'); digits++; }
      if (!digits) return "missing light number after '-'";
    }
    if (first < 1 || first > NUM_LEDS || last < 1 || last > NUM_LEDS) return "light numbers must be 1 to 65";
    while (pos < len && text[pos] == ' ') pos++;
    if (pos < len && text[pos] != ',') return "separate ranges with commas, e.g. A1-12, B30-65";

    outRanges[count++] = { (uint8_t)out, (uint8_t)(first - 1), (uint8_t)(last - 1) };
  }
  if (count == 0) return "enter at least one range, e.g. A1-12";
  return nullptr;
}

// "A1-12, B30-65"
String rangesText(const ZoneRange* ranges, uint8_t count) {
  String s;
  for (int r = 0; r < count; r++) {
    if (r) s += ", ";
    s += OUTPUT_LETTERS[ranges[r].output];
    s += ranges[r].first + 1;
    if (ranges[r].last != ranges[r].first) {
      s += "-";
      s += ranges[r].last + 1;
    }
  }
  return s;
}

// Copies a name from the web into a fixed buffer, replacing characters that would
// break JSON. Returns false if it ends up empty.
bool copyName(const String& from, char* to) {
  int n = 0;
  for (unsigned int i = 0; i < from.length() && n < NAME_LEN - 1; i++) {
    char c = from[i];
    if (c < 32 || c > 126) continue;
    if (c == '"' || c == '\\') c = '\'';
    to[n++] = c;
  }
  while (n > 0 && to[n - 1] == ' ') n--;
  to[n] = 0;
  return n > 0;
}

// ---- Create / change / delete (returns an error message or nullptr) ----

// id < 0 = new zone. On success, newId is the zone's slot.
const char* saveZone(int id, const String& name, const String& rangesStr, int& newId) {
  ZoneRange ranges[MAX_ZONE_RANGES];
  uint8_t count;
  const char* err = parseRanges(rangesStr, ranges, count);
  if (err) return err;

  char cleanName[NAME_LEN];
  if (!copyName(name, cleanName)) return "enter a zone name";

  if (id < 0) {
    for (int z = 0; z < MAX_ZONES && id < 0; z++) if (!zones[z].used) id = z;
    if (id < 0) return "all 12 zones are used; delete one first";
    states[CH_ZONE0 + id] = ZONE_DEFAULT_STATE;
  } else if (id >= MAX_ZONES || !zones[id].used) {
    return "zone not found";
  }

  ZoneDef& zd = zones[id];
  zd.used = true;
  strcpy(zd.name, cleanName);
  zd.rangeCount = count;
  for (int r = 0; r < count; r++) zd.ranges[r] = ranges[r];
  rebuildZone(id);
  resetPhase(CH_ZONE0 + id);

  markStateChanged();
  newId = id;
  return nullptr;
}

const char* deleteZone(int id) {
  if (id < 0 || id >= MAX_ZONES || !zones[id].used) return "zone not found";
  zones[id].used = false;
  zones[id].name[0] = 0;
  zones[id].rangeCount = 0;
  states[CH_ZONE0 + id] = ZONE_DEFAULT_STATE;
  rebuildZone(id);
  markStateChanged();
  return nullptr;
}

// ---- File ----

void loadZones() {
  if (!storageOk || !FFat.exists(ZONES_FILE)) return;
  File f = FFat.open(ZONES_FILE, "r");
  if (!f) return;
  FileHeader h;
  uint16_t recordSize = sizeof(ZoneDef) + sizeof(LightState);
  if (f.read((uint8_t*)&h, sizeof(h)) != sizeof(h) || h.magic != ZONES_MAGIC ||
      h.version != ZONES_VERSION || h.recordSize != recordSize) {
    Serial.println("Zones: file is from a different firmware layout, starting with no zones");
    f.close();
    return;
  }
  int loaded = 0;
  for (int z = 0; z < h.count && z < MAX_ZONES; z++) {
    if (f.read((uint8_t*)&zones[z], sizeof(ZoneDef)) != sizeof(ZoneDef)) break;
    if (f.read((uint8_t*)&states[CH_ZONE0 + z], sizeof(LightState)) != sizeof(LightState)) break;
    LightState& s = states[CH_ZONE0 + z];
    if (s.mode >= MODE_COUNT) s.mode = MODE_SOLID;
    s.fadeSeconds = constrain(s.fadeSeconds, FADE_SECONDS_MIN, FADE_SECONDS_MAX);
    s.speed = constrain(s.speed, SPEED_MIN, SPEED_MAX);
    s.size = constrain(s.size, SIZE_MIN, SIZE_MAX);
    zones[z].name[NAME_LEN - 1] = 0;
    if (zones[z].rangeCount > MAX_ZONE_RANGES) zones[z].used = false;
    rebuildZone(z);
    if (zones[z].used) loaded++;
  }
  f.close();
  Serial.printf("Zones: %d loaded\n", loaded);
  for (int z = 0; z < MAX_ZONES; z++) {
    if (!zones[z].used) continue;
    Serial.printf("  %s = %s (%u lights), %s\n", zones[z].name,
                  rangesText(zones[z].ranges, zones[z].rangeCount).c_str(), zoneLen[z],
                  states[CH_ZONE0 + z].on ? "ON" : "OFF");
  }
}

// Writes all zones and their settings. Called from handleStateSave() (delayed save).
void saveZonesFile() {
  if (!storageOk) return;
  File f = FFat.open(ZONES_FILE, "w");
  if (!f) {
    Serial.println("Zones: could not write file");
    return;
  }
  FileHeader h = { ZONES_MAGIC, ZONES_VERSION,
                   (uint16_t)(sizeof(ZoneDef) + sizeof(LightState)), MAX_ZONES };
  f.write((const uint8_t*)&h, sizeof(h));
  for (int z = 0; z < MAX_ZONES; z++) {
    f.write((const uint8_t*)&zones[z], sizeof(ZoneDef));
    f.write((const uint8_t*)&states[CH_ZONE0 + z], sizeof(LightState));
  }
  f.close();
}
