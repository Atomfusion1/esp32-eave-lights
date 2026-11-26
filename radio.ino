// radio — sends "radio" strands to hub boards by ESP-NOW, and collects their reports.
//
// A strand set to Radio (Setup > Strands) is drawn here like any other strand; its colors
// (after the power caps, in the strand's color order) are broadcast to every hub on the
// Wi-Fi channel this board is on (the home network's, or the hotspot's). No router is
// involved and no pairing is needed: a hub drives the strand letters set in its sketch
// (EaveHub/EaveHub.ino) and finds the channel by itself.
//
// Sending: a strand is sent when it changes, and at least once a second (so a hub that
// restarts gets the picture, and can find the channel). Long range = 1 Mbps, up to 25
// frames/s; Fast = 6 Mbps, up to 50 frames/s (shorter reach). Saved in Preferences "rrate".
// Hubs broadcast a report every second: their name, strands, how well they hear this
// board (signal, % of frames received). Shown on the page (Setup > Strands).
//
// Packets (must match EaveHub.ino, RADIO_VERSION):
//   frame : RadioFrameHdr + n x 3 bytes (wire order), a strand split into chunks
//   report: RadioReport (hub -> here)

#define RADIO_MAGIC        0x52455645UL   // "EVER" (Eave lights radio)
#define RADIO_VERSION      1
#define RADIO_TYPE_FRAME   1
#define RADIO_TYPE_REPORT  2
#define RADIO_CHUNK_LIGHTS 200            // lights per packet (600 bytes of color)
#define RADIO_KEEPALIVE_MS 1000
#define MAX_HUBS           4

struct __attribute__((packed)) RadioFrameHdr {
  uint32_t magic;
  uint8_t  version;
  uint8_t  type;      // RADIO_TYPE_FRAME
  uint8_t  strand;    // 0 = A ... 5 = F
  uint8_t  flags;     // bit 0: last chunk of this frame
  uint16_t seq;       // frame number (per strand), same for all chunks of a frame
  uint16_t count;     // lights on the strand
  uint16_t first;     // first light in this packet (0-based)
  uint16_t n;         // lights in this packet
};

struct __attribute__((packed)) RadioReport {
  uint32_t magic;
  uint8_t  version;
  uint8_t  type;        // RADIO_TYPE_REPORT
  uint8_t  strandMask;  // bit o = the hub drives strand o
  int8_t   rssi;        // how strongly the hub hears this board (dBm, average)
  uint8_t  pct;         // % of frames received over the last few seconds (255 = no data)
  uint8_t  channel;
  uint16_t spare;
  uint32_t uptimeS;
  char     name[20];
  char     fw[12];
};

struct HubInfo {
  bool     used;
  uint8_t  mac[6];
  char     name[20];
  char     fw[12];
  uint8_t  strandMask;
  int8_t   rssiThere;   // the hub hearing us
  int8_t   rssiHere;    // us hearing the hub
  uint8_t  pct;
  uint32_t uptimeS;
  uint32_t lastMs;
};

const uint8_t RADIO_BROADCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
bool     radioOk     = false;
uint8_t  radioRate   = 0;          // 0 = long range (1 Mbps), 1 = fast (6 Mbps)
uint8_t  radioIf     = WIFI_IF_STA;
uint16_t radioSeq[NUM_OUTPUTS] = { 0 };
uint32_t radioLastSendMs[NUM_OUTPUTS] = { 0 };
uint32_t radioLastFrameMs = 0;
uint32_t radioSent = 0, radioSendFails = 0;
CRGB*    radioLast = nullptr;      // last sent colors per strand [NUM_OUTPUTS][MAX_LEDS] (PSRAM)
uint8_t* radioPkt  = nullptr;      // packet being built
HubInfo  hubs[MAX_HUBS];
portMUX_TYPE hubsMux = portMUX_INITIALIZER_UNLOCKED;

const char* RADIO_RATE_NAMES[] = { "long", "fast" };

bool anyRadioStrand() {
  for (int o = 0; o < NUM_OUTPUTS; o++) if (strandCfg[o].type == STRAND_RADIO) return true;
  return false;
}

// Called by ESP-NOW (Wi-Fi task) for every packet received: keeps hub reports.
void onRadioReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len != (int)sizeof(RadioReport)) return;
  RadioReport r;
  memcpy(&r, data, sizeof(r));
  if (r.magic != RADIO_MAGIC || r.version != RADIO_VERSION || r.type != RADIO_TYPE_REPORT) return;
  portENTER_CRITICAL(&hubsMux);
  int slot = -1, oldest = 0;
  for (int h = 0; h < MAX_HUBS; h++) {
    if (hubs[h].used && memcmp(hubs[h].mac, info->src_addr, 6) == 0) { slot = h; break; }
    if (slot < 0 && !hubs[h].used) slot = h;
  }
  if (slot < 0) {   // table full: replace the one heard from longest ago
    for (int h = 1; h < MAX_HUBS; h++) if ((int32_t)(hubs[h].lastMs - hubs[oldest].lastMs) < 0) oldest = h;
    slot = oldest;
  }
  HubInfo& hb = hubs[slot];
  hb.used = true;
  memcpy(hb.mac, info->src_addr, 6);
  memcpy(hb.name, r.name, sizeof(hb.name));
  hb.name[sizeof(hb.name) - 1] = 0;
  memcpy(hb.fw, r.fw, sizeof(hb.fw));
  hb.fw[sizeof(hb.fw) - 1] = 0;
  hb.strandMask = r.strandMask;
  hb.rssiThere = r.rssi;
  hb.rssiHere = info->rx_ctrl ? info->rx_ctrl->rssi : 0;
  hb.pct = r.pct;
  hb.uptimeS = r.uptimeS;
  hb.lastMs = millis();
  portEXIT_CRITICAL(&hubsMux);
}

// The interface ESP-NOW sends on: the hotspot's while it runs, else the home network's.
uint8_t wantedRadioIf() { return (WiFi.getMode() & WIFI_MODE_AP) ? WIFI_IF_AP : WIFI_IF_STA; }

void applyRadioPeer() {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, RADIO_BROADCAST, 6);
  peer.channel = 0;                     // whatever channel the interface is on
  peer.ifidx = (wifi_interface_t)radioIf;
  peer.encrypt = false;
  if (esp_now_is_peer_exist(RADIO_BROADCAST)) esp_now_mod_peer(&peer);
  else esp_now_add_peer(&peer);
  esp_now_rate_config_t rc = {};
  rc.phymode = radioRate ? WIFI_PHY_MODE_11G : WIFI_PHY_MODE_11B;
  rc.rate = radioRate ? WIFI_PHY_RATE_6M : WIFI_PHY_RATE_1M_L;
  esp_now_set_peer_rate_config(RADIO_BROADCAST, &rc);
}

// After startWifi(): starts ESP-NOW (always, so hubs can be seen before any strand is Radio).
void startRadio() {
  Preferences p;
  p.begin(PREFS_NAMESPACE, true);
  radioRate = p.getUChar("rrate", 0) ? 1 : 0;
  p.end();
  radioLast = (CRGB*)bigAlloc(sizeof(CRGB) * NUM_OUTPUTS * MAX_LEDS);
  radioPkt  = (uint8_t*)bigAlloc(sizeof(RadioFrameHdr) + RADIO_CHUNK_LIGHTS * 3);
  WiFi.setSleep(false);               // hear hub reports right away (no modem sleep)
  if (esp_now_init() != ESP_OK) {
    Serial.println("Radio: ESP-NOW did not start");
    return;
  }
  esp_now_register_recv_cb(onRadioReceive);
  radioIf = wantedRadioIf();
  applyRadioPeer();
  radioOk = true;
  Serial.printf("Radio: ESP-NOW ready (%s range)\n", radioRate ? "fast, shorter" : "long");
}

// "long" / "fast" from the page. Returns false for anything else.
bool setRadioRate(const String& v) {
  int r = v == "long" ? 0 : v == "fast" ? 1 : -1;
  if (r < 0) return false;
  radioRate = r;
  if (radioOk) applyRadioPeer();
  if (!restoreBlocksSaving()) {
    Preferences p;
    p.begin(PREFS_NAMESPACE, false);
    p.putUChar("rrate", radioRate);
    p.end();
  }
  return true;
}

// Sends one strand (all its chunks). Colors from out[] in the strand's color order.
void sendRadioStrand(int o) {
  int count = ledCount[o];
  uint8_t order = strandCfg[o].order;
  uint16_t seq = ++radioSeq[o];
  for (int first = 0; first < count; first += RADIO_CHUNK_LIGHTS) {
    int n = min(RADIO_CHUNK_LIGHTS, count - first);
    RadioFrameHdr h = { RADIO_MAGIC, RADIO_VERSION, RADIO_TYPE_FRAME, (uint8_t)o,
                        (uint8_t)(first + n >= count ? 1 : 0), seq, (uint16_t)count, (uint16_t)first, (uint16_t)n };
    memcpy(radioPkt, &h, sizeof(h));
    uint8_t* p = radioPkt + sizeof(h);
    for (int i = 0; i < n; i++) {
      CRGB c = orderColor(out[o][first + i], order);
      *p++ = c.r; *p++ = c.g; *p++ = c.b;
    }
    if (esp_now_send(RADIO_BROADCAST, radioPkt, sizeof(h) + n * 3) == ESP_OK) radioSent++;
    else radioSendFails++;
  }
}

// Called after every frame (showFrame): sends the radio strands that changed, or are due.
void radioAfterFrame() {
  if (!radioOk) return;
  uint8_t want = wantedRadioIf();
  if (want != radioIf) { radioIf = want; applyRadioPeer(); }   // hotspot started / stopped
  uint32_t now = millis();
  uint32_t minGap = radioRate ? 18 : 38;   // up to ~50 / ~25 frames per second
  if (now - radioLastFrameMs < minGap) return;
  radioLastFrameMs = now;
  for (int o = 0; o < NUM_OUTPUTS; o++) {
    if (strandCfg[o].type != STRAND_RADIO) continue;
    CRGB* last = radioLast + o * MAX_LEDS;
    size_t bytes = ledCount[o] * sizeof(CRGB);
    bool changed = memcmp(last, out[o], bytes) != 0;
    if (!changed && now - radioLastSendMs[o] < RADIO_KEEPALIVE_MS) continue;
    memcpy(last, out[o], bytes);
    radioLastSendMs[o] = now;
    sendRadioStrand(o);
  }
}

// "radio":{...} for the state: channel, speed, and the hubs heard in the last minute.
String radioJson() {
  String j = "{\"ok\":";
  j += radioOk ? "true" : "false";
  j += ",\"channel\":" + String(WiFi.channel()) + ",\"rate\":\"" + RADIO_RATE_NAMES[radioRate] + "\",\"hubs\":[";
  HubInfo copy[MAX_HUBS];
  portENTER_CRITICAL(&hubsMux);
  memcpy(copy, hubs, sizeof(copy));
  portEXIT_CRITICAL(&hubsMux);
  uint32_t now = millis();
  bool first = true;
  for (int h = 0; h < MAX_HUBS; h++) {
    const HubInfo& hb = copy[h];
    if (!hb.used || now - hb.lastMs > 60000) continue;
    String letters;
    for (int o = 0; o < NUM_OUTPUTS; o++) if (hb.strandMask & (1 << o)) letters += OUTPUT_LETTERS[o];
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X", hb.mac[0], hb.mac[1], hb.mac[2], hb.mac[3], hb.mac[4], hb.mac[5]);
    if (!first) j += ",";
    first = false;
    j += "{\"mac\":\"" + String(mac) + "\",\"name\":\"" + String(hb.name) + "\",\"fw\":\"" + String(hb.fw) +
         "\",\"strands\":\"" + letters + "\",\"rssi\":" + String(hb.rssiThere) + ",\"rssiHere\":" + String(hb.rssiHere) +
         ",\"pct\":" + (hb.pct > 100 ? String("null") : String(hb.pct)) + ",\"uptime\":" + String(hb.uptimeS) +
         ",\"age\":" + String((now - hb.lastMs) / 1000) + "}";
  }
  j += "]}";
  return j;
}
