// zones - named light ranges with their own settings, saved to /zones.bin.
//
// Ranges are written like the page shows them: "A1-12, B30-65", "A12-1" (backwards),
// "B5" (one light). Numbers are 1-based light numbers; stored 0-based.
//
// /zones.bin: header (magic, version, record size, count), then per zone slot:
// ZoneDef followed by that zone's LightState. Older versions are converted on load:
// v1 (before 0.12.0) 8-bit light numbers, v2 (before 0.15.0) settings without the color
// list. Any other mismatch means the file is ignored and zones start empty.

const char*    ZONES_FILE    = "/zones.bin";
const uint32_t ZONES_MAGIC   = 0x5A4E4F5A;  // "ZONZ"
const uint16_t ZONES_VERSION = 3;

// Version 1 layout (8-bit light numbers), only used to convert old files.
struct ZoneRangeV1 { uint8_t output; uint8_t first; uint8_t last; };
struct ZoneDefV1 {
  bool        used;
  char        name[NAME_LEN];
  uint8_t     rangeCount;
  ZoneRangeV1 ranges[MAX_ZONE_RANGES];
};

void initZones() {
  for (int z = 0; z < MAX_ZONES; z++) {
    zones[z].used = false;
    zones[z].name[0] = 0;
    zones[z].rangeCount = 0;
    zoneLen[z] = 0;
    zoneHidden[z] = 0;
    zoneRank[z] = z;
    zoneMirror[z] = -1;
    states[CH_ZONE0 + z] = ZONE_DEFAULT_STATE;
  }
}

// Rebuilds zone z's light order from its ranges. Lights past the end of a strand
// (if a strand was made shorter) are left out.
void rebuildZone(int z) {
  uint16_t n = 0;
  if (zones[z].used) {
    for (int r = 0; r < zones[z].rangeCount; r++) {
      const ZoneRange& zr = zones[z].ranges[r];
      int step = (zr.first <= zr.last) ? 1 : -1;
      for (int i = zr.first; n < MAX_TOTAL_LEDS; i += step) {
        if (i < ledCount[zr.output]) zonePoints[z][n++] = { zr.output, (uint16_t)i };
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
    if (out < 0) return "each range must start with a strand letter A-F, e.g. A1-12";

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
    if (first < 1 || first > ledCount[out] || last < 1 || last > ledCount[out]) {
      static char msg[64];
      snprintf(msg, sizeof(msg), "strand %c has lights 1 to %u", OUTPUT_LETTERS[out], ledCount[out]);
      return msg;
    }
    while (pos < len && text[pos] == ' ') pos++;
    if (pos < len && text[pos] != ',') return "separate ranges with commas, e.g. A1-12, B30-65";

    outRanges[count++] = { (uint8_t)out, (uint16_t)(first - 1), (uint16_t)(last - 1) };
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
    // A new zone goes to the end of the list.
    uint8_t last = 0;
    for (int z = 0; z < MAX_ZONES; z++) if (zones[z].used && zoneRank[z] >= last) last = zoneRank[z] + 1;
    zoneRank[id] = last;
    zoneHidden[id] = 0;
    zoneMirror[id] = -1;
  } else if (id >= MAX_ZONES || !zones[id].used) {
    return "zone not found";
  }

  ZoneDef& zd = zones[id];
  zd.used = true;
  strcpy(zd.name, cleanName);
  zd.rangeCount = count;
  for (int r = 0; r < count; r++) zd.ranges[r] = ranges[r];
  rebuildZone(id);
  rebuildAllZonePaths();   // paths follow the zone's new lights
  // its lights changed: strands skip the new ones (never-lit or mirror zone, or one mirrored)
  bool mirrored = false;
  for (int z = 0; z < MAX_ZONES; z++) if (zones[z].used && zoneMirror[z] == id) mirrored = true;
  if (zoneHidden[id] || zoneMirror[id] >= 0 || mirrored) initPaths();
  resetPhase(CH_ZONE0 + id);
  saveZoneMeta();

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
  // mirrors of it stop mirroring; its own mirror setting goes
  bool linesChanged = zoneHidden[id] || zoneMirror[id] >= 0;
  for (int z = 0; z < MAX_ZONES; z++) if (zoneMirror[z] == id) { zoneMirror[z] = -1; linesChanged = true; }
  zoneHidden[id] = 0;
  zoneMirror[id] = -1;
  if (linesChanged) initPaths();   // its lights go back into the strands and Whole house
  saveZoneMeta();
  // Paths drop their steps for it, and scenes forget it, so a new zone that gets this
  // slot later isn't pulled into them.
  for (int p = 0; p < MAX_PATHS; p++) {
    PathDef& pd = pathDefs[p];
    if (!pd.used) continue;
    int n = 0;
    for (int k = 0; k < pd.stepCount; k++) if (pd.steps[k].zone != id) pd.steps[n++] = pd.steps[k];
    pd.stepCount = n;
  }
  forgetChannel(CH_ZONE0 + id, ZONE_DEFAULT_STATE);
  rebuildAllZonePaths();
  markStateChanged();
  return nullptr;
}

// ---- File ----

void loadZones() {
  if (!storageOk || !FFat.exists(ZONES_FILE)) return;
  File f = FFat.open(ZONES_FILE, "r");
  if (!f) return;
  FileHeader h;
  bool okHeader = f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == ZONES_MAGIC;
  // v1: 8-bit light numbers + old settings; v2: 16-bit numbers + old settings;
  // v3 (current): 16-bit numbers + settings with the color list.
  bool isV1 = okHeader && h.version == 1 &&
              h.recordSize == sizeof(ZoneDefV1) + sizeof(LightStateV1);
  bool isV2 = okHeader && h.version == 2 &&
              h.recordSize == sizeof(ZoneDef) + sizeof(LightStateV1);
  bool isV3 = okHeader && h.version == ZONES_VERSION &&
              h.recordSize == sizeof(ZoneDef) + sizeof(LightState);
  if (!isV1 && !isV2 && !isV3) {
    Serial.println("Zones: file is from a different firmware layout, starting with no zones");
    f.close();
    return;
  }
  bool converted = isV1 || isV2;
  int loaded = 0;
  for (int z = 0; z < h.count && z < MAX_ZONES; z++) {
    if (isV1) {
      // Convert the old 8-bit light numbers.
      ZoneDefV1 old;
      if (f.read((uint8_t*)&old, sizeof(old)) != sizeof(old)) break;
      zones[z].used = old.used;
      memcpy(zones[z].name, old.name, NAME_LEN);
      zones[z].rangeCount = old.rangeCount;
      for (int r = 0; r < MAX_ZONE_RANGES; r++) {
        zones[z].ranges[r] = { old.ranges[r].output, old.ranges[r].first, old.ranges[r].last };
      }
    } else {
      if (f.read((uint8_t*)&zones[z], sizeof(ZoneDef)) != sizeof(ZoneDef)) break;
    }
    if (converted) {
      LightStateV1 old;
      if (f.read((uint8_t*)&old, sizeof(old)) != sizeof(old)) break;
      states[CH_ZONE0 + z] = upgradeState(old);
    } else {
      if (f.read((uint8_t*)&states[CH_ZONE0 + z], sizeof(LightState)) != sizeof(LightState)) break;
    }
    sanitizeState(states[CH_ZONE0 + z]);
    zones[z].name[NAME_LEN - 1] = 0;
    if (zones[z].rangeCount > MAX_ZONE_RANGES) zones[z].used = false;
    rebuildZone(z);
    if (zones[z].used) loaded++;
  }
  f.close();
  loadZoneMeta();
  initPaths();   // strands and Whole house skip the "never light" zones
  Serial.printf("Zones: %d loaded%s\n", loaded, converted ? " (converted from the old format)" : "");
  if (converted) markStateChanged();   // write it back in the new format
  for (int z = 0; z < MAX_ZONES; z++) {
    if (!zones[z].used) continue;
    Serial.printf("  %s = %s (%u lights), %s\n", zones[z].name,
                  rangesText(zones[z].ranges, zones[z].rangeCount).c_str(), zoneLen[z],
                  zoneHidden[z] ? "NEVER LIT" : states[CH_ZONE0 + z].on ? "ON" : "OFF");
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

// ---- "Never light" zones, mirrors and list order (/zonemeta.bin) ----
// Per zone slot: hidden (0/1), rank (list position, lower first), mirror (source slot + 1,
// 0 = none; v2, v0.41.0). Version 1 records had only the first two bytes.

const char*    ZONEMETA_FILE  = "/zonemeta.bin";
const uint32_t ZONEMETA_MAGIC = 0x4154455A;  // "ZETA"

// True if zone z is a working mirror (its source exists).
bool zoneIsMirror(int z) {
  int m = zoneMirror[z];
  return zones[z].used && !zoneHidden[z] && m >= 0 && m < MAX_ZONES && m != z && zones[m].used;
}

// Marks every light of a "never light" zone, and of a mirror zone. Lights past a strand's
// end are ignored.
void rebuildHiddenLights() {
  memset(lightHidden, 0, sizeof(lightHidden));
  memset(lightMirrored, 0, sizeof(lightMirrored));
  for (int z = 0; z < MAX_ZONES; z++) {
    if (!zones[z].used) continue;
    bool hide = zoneHidden[z], mirror = zoneIsMirror(z);
    if (!hide && !mirror) continue;
    for (int r = 0; r < zones[z].rangeCount; r++) {
      const ZoneRange& zr = zones[z].ranges[r];
      int a = min(zr.first, zr.last), b = max(zr.first, zr.last);
      for (int i = a; i <= b && i < ledCount[zr.output]; i++) {
        if (hide) lightHidden[zr.output][i] = true;
        else lightMirrored[zr.output][i] = true;
      }
    }
  }
}

// After all layers are drawn: each mirror zone's lights take the colors of its source's
// lights, position by position. Lights past the source's length are dark.
void applyZoneMirrors() {
  for (int z = 0; z < MAX_ZONES; z++) {
    if (!zoneIsMirror(z)) continue;
    int m = zoneMirror[z];
    for (int i = 0; i < zoneLen[z]; i++) {
      const PathPoint& d = zonePoints[z][i];
      CRGB c(0, 0, 0);
      if (i < zoneLen[m]) { const PathPoint& s = zonePoints[m][i]; c = frame[s.output][s.index]; }
      frame[d.output][d.index] = c;
    }
  }
}

// Sets zone id to mirror zone src (-1 = no mirror). A mirror can't be mirrored, a zone
// that others mirror can't become a mirror, and a "never light" zone can't mirror.
const char* setZoneMirror(int id, int src) {
  if (id < 0 || id >= MAX_ZONES || !zones[id].used) return "zone not found";
  if (src >= 0) {
    if (src >= MAX_ZONES || !zones[src].used) return "the zone to mirror was not found";
    if (src == id) return "a zone can't mirror itself";
    if (zoneHidden[id]) return "a never-lit zone can't be a mirror";
    if (zoneMirror[src] >= 0) return "that zone is itself a mirror: pick the zone it mirrors";
    for (int z = 0; z < MAX_ZONES; z++)
      if (z != id && zones[z].used && zoneMirror[z] == id) return "other zones mirror this one, so it can't be a mirror";
  } else {
    src = -1;
  }
  if (zoneMirror[id] == src) return nullptr;
  zoneMirror[id] = src;
  initPaths();   // strand lines leave out (or take back) its lights
  saveZoneMeta();
  return nullptr;
}

// Used zone slots in list order. Returns how many.
int zonesInOrder(uint8_t* slots) {
  int n = 0;
  for (int z = 0; z < MAX_ZONES; z++) if (zones[z].used) slots[n++] = z;
  for (int i = 1; i < n; i++) {           // insertion sort by rank, then slot
    uint8_t s = slots[i];
    int j = i - 1;
    while (j >= 0 && (zoneRank[slots[j]] > zoneRank[s] ||
                      (zoneRank[slots[j]] == zoneRank[s] && slots[j] > s))) {
      slots[j + 1] = slots[j];
      j--;
    }
    slots[j + 1] = s;
  }
  return n;
}

// Renumbers the ranks 0, 1, 2... in the current order.
void normalizeZoneRanks() {
  uint8_t slots[MAX_ZONES];
  int n = zonesInOrder(slots);
  for (int i = 0; i < n; i++) zoneRank[slots[i]] = i;
}

const char* setZoneHidden(int id, bool hidden) {
  if (id < 0 || id >= MAX_ZONES || !zones[id].used) return "zone not found";
  if ((zoneHidden[id] != 0) == hidden) return nullptr;
  zoneHidden[id] = hidden ? 1 : 0;
  initPaths();
  saveZoneMeta();
  return nullptr;
}

// dir < 0 = up one place in the list, otherwise down one place.
const char* moveZone(int id, int dir) {
  if (id < 0 || id >= MAX_ZONES || !zones[id].used) return "zone not found";
  normalizeZoneRanks();
  uint8_t slots[MAX_ZONES];
  int n = zonesInOrder(slots);
  int pos = zoneRank[id], other = pos + (dir < 0 ? -1 : 1);
  if (other < 0 || other >= n) return nullptr;   // already first / last
  zoneRank[slots[other]] = pos;
  zoneRank[id] = other;
  saveZoneMeta();
  return nullptr;
}

void loadZoneMeta() {
  for (int z = 0; z < MAX_ZONES; z++) { zoneHidden[z] = 0; zoneRank[z] = z; zoneMirror[z] = -1; }
  if (storageOk && FFat.exists(ZONEMETA_FILE)) {
    File f = FFat.open(ZONEMETA_FILE, "r");
    FileHeader h;
    // record size 2 (v1: hidden, rank) or 3 (v2: + mirror)
    if (f && f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == ZONEMETA_MAGIC &&
        (h.recordSize == 2 || h.recordSize == 3)) {
      for (int z = 0; z < h.count && z < MAX_ZONES; z++) {
        uint8_t rec[3] = { 0, 0, 0 };
        if (f.read(rec, h.recordSize) != h.recordSize) break;
        zoneHidden[z] = rec[0] ? 1 : 0;
        zoneRank[z] = rec[1];
        zoneMirror[z] = (rec[2] >= 1 && rec[2] <= MAX_ZONES) ? rec[2] - 1 : -1;
      }
    }
    if (f) f.close();
  }
  for (int z = 0; z < MAX_ZONES; z++) if (!zones[z].used) { zoneHidden[z] = 0; zoneMirror[z] = -1; }
  normalizeZoneRanks();
}

void saveZoneMeta() {
  if (!storageOk || savesBlocked) return;
  File f = FFat.open(ZONEMETA_FILE, "w");
  if (!f) return;
  FileHeader h = { ZONEMETA_MAGIC, 2, 3, MAX_ZONES };
  f.write((const uint8_t*)&h, sizeof(h));
  for (int z = 0; z < MAX_ZONES; z++) {
    uint8_t rec[3] = { zoneHidden[z], zoneRank[z], (uint8_t)(zoneMirror[z] >= 0 ? zoneMirror[z] + 1 : 0) };
    f.write(rec, 3);
  }
  f.close();
}
