// EaveHub — the second board ("hub") near the yard lights: receives radio strands from
// the eave board by ESP-NOW and drives them. No Wi-Fi password and no router needed.
//
// How it works:
//  - Listens on Wi-Fi channels 1-13 in turn until it hears the eave board (which sends
//    every radio strand at least once a second), then stays on that channel.
//  - Drives the strands set below (e.g. C = palm tree on GPIO 8, D = oak floods on GPIO 9).
//    The eave board already applied power caps and the color order: colors go out as is.
//  - Signal lost: holds the last picture SIGNAL_LOST_MS, then fades out over FADE_OUT_MS,
//    and listens on all channels again.
//  - Every second it reports to the eave board how well it hears it (signal and % of
//    frames). The eave board's page shows that under Setup > Strands > Radio hubs, so a
//    bare hub board on a battery pack is a range test.
//  - The board's RGB LED: blue blink = looking for the eave board, red blink = signal
//    lost, otherwise it copies light 1 of STATUS_LED_COPIES (or dim green).
//
// On the eave board: Setup > Strands, set the same letters to "Radio".
//
// Board settings (Tools menu), same as the eave board (see ../AGENTS.md):
//   ESP32S3 Dev Module, USB CDC On Boot: Disabled, Flash Size 16MB, PSRAM: OPI PSRAM.
//   Any partition scheme works. Upload over USB (this board isn't on the home Wi-Fi).
// Serial (115200) prints the link every 5 seconds.
//
// Must match the eave board's radio.ino: RADIO_MAGIC, RADIO_VERSION and the packet layouts.

#include <FastLED.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>

// ===================== Settings for this hub =====================
#define HUB_NAME        "Palm box"   // shown on the eave board's page (up to 19 characters)
#define HUB_FW          "hub 1.0.0"

// Up to two strands. Strand letter as set on the eave board, and the GPIO it is wired to.
// Set OUT2_STRAND to 0 if only one strand is connected.
#define OUT1_STRAND     'C'
#define OUT1_PIN        8
#define OUT2_STRAND     'D'
#define OUT2_PIN        9
#define MAX_LIGHTS      400          // per strand (the eave board's maximum)

// The board's own RGB LED (WS2812). -1 = don't use it.
#define STATUS_LED_PIN    48         // 38 on some boards
#define STATUS_LED_COPIES 'D'        // copies light 1 of this strand while linked (0 = dim green)
#define STATUS_LED_LEVEL  64         // 25 %: it is very bright up close

#define SIGNAL_LOST_MS  3000         // hold the last picture this long without frames
#define FADE_OUT_MS     2000         // then fade out
#define SCAN_STEP_MS    300          // looking for the eave board: time per channel
// ================================================================

#define RADIO_MAGIC        0x52455645UL   // "EVER"
#define RADIO_VERSION      1
#define RADIO_TYPE_FRAME   1
#define RADIO_TYPE_REPORT  2

struct __attribute__((packed)) RadioFrameHdr {
  uint32_t magic;
  uint8_t  version;
  uint8_t  type;      // RADIO_TYPE_FRAME
  uint8_t  strand;    // 0 = A ... 5 = F
  uint8_t  flags;     // bit 0: last chunk of this frame
  uint16_t seq;       // frame number (per strand)
  uint16_t count;     // lights on the strand
  uint16_t first;     // first light in this packet (0-based)
  uint16_t n;         // lights in this packet
};

struct __attribute__((packed)) RadioReport {
  uint32_t magic;
  uint8_t  version;
  uint8_t  type;        // RADIO_TYPE_REPORT
  uint8_t  strandMask;  // bit o = this hub drives strand o
  int8_t   rssi;        // how strongly this hub hears the eave board (dBm, average)
  uint8_t  pct;         // % of frames received over the last few seconds (255 = no data)
  uint8_t  channel;
  uint16_t spare;
  uint32_t uptimeS;
  char     name[20];
  char     fw[12];
};

#if OUT2_STRAND
#define NUM_OUTS 2
#else
#define NUM_OUTS 1
#endif

const uint8_t BROADCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const int     OUT_STRAND[2] = { OUT1_STRAND - 'A', OUT2_STRAND ? OUT2_STRAND - 'A' : -1 };

CRGB leds[NUM_OUTS][MAX_LIGHTS];      // what FastLED sends
CRGB staged[NUM_OUTS][MAX_LIGHTS];    // latest received (written by the radio callback)
CLEDController* ctl[NUM_OUTS];
CRGB statusLed[1];
CLEDController* statusCtl = nullptr;

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
// written in the callback (under mux)
uint16_t stagedCount[NUM_OUTS] = { 0 };
bool     stagedDirty[NUM_OUTS] = { false };
uint32_t lastFrameMs[NUM_OUTS] = { 0 };
uint32_t framesGot[NUM_OUTS]   = { 0 };
uint16_t lastSeq[NUM_OUTS]     = { 0 };
bool     seqKnown[NUM_OUTS]    = { false };
uint32_t seqSpan[NUM_OUTS]     = { 0 };   // frames sent by the eave board (from seq numbers)
volatile uint32_t lastHeardMs = 0;        // any frame from the eave board
int32_t  rssiAvg16 = -90 * 16;            // running average, x16

uint16_t shownCount[NUM_OUTS] = { 0 };
bool     darkDone[NUM_OUTS]   = { false };   // faded out after losing the signal
uint8_t  channel = 1;
bool     linked = false;
uint32_t scanMs = 0, reportMs = 0, printMs = 0, showMs = 0;
uint8_t  pct = 255;
uint32_t pctGot0[NUM_OUTS] = { 0 }, pctSpan0[NUM_OUTS] = { 0 }, pctMs = 0;

int outputFor(int strand) {
  for (int k = 0; k < NUM_OUTS; k++) if (OUT_STRAND[k] == strand) return k;
  return -1;
}

// ESP-NOW callback (Wi-Fi task): copies colors of our strands into staged[].
void onRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len < (int)sizeof(RadioFrameHdr)) return;
  RadioFrameHdr h;
  memcpy(&h, data, sizeof(h));
  if (h.magic != RADIO_MAGIC || h.version != RADIO_VERSION || h.type != RADIO_TYPE_FRAME) return;
  if ((int)sizeof(h) + h.n * 3 > len) return;
  uint32_t now = millis();
  lastHeardMs = now;
  int k = outputFor(h.strand);
  portENTER_CRITICAL(&mux);
  if (info->rx_ctrl) rssiAvg16 += (info->rx_ctrl->rssi * 16 - rssiAvg16) / 8;
  if (k >= 0) {
    const uint8_t* p = data + sizeof(h);
    for (int i = 0; i < h.n; i++) {
      int idx = h.first + i;
      if (idx >= MAX_LIGHTS) break;
      staged[k][idx] = CRGB(p[0], p[1], p[2]);
      p += 3;
    }
    stagedCount[k] = min((int)h.count, MAX_LIGHTS);
    stagedDirty[k] = true;
    lastFrameMs[k] = now;
    if (h.flags & 1) {   // a whole frame arrived (its last chunk)
      framesGot[k]++;
      if (seqKnown[k]) {
        uint16_t d = h.seq - lastSeq[k];
        if (d > 0 && d < 1000) seqSpan[k] += d;
      } else {
        seqSpan[k]++;
      }
      lastSeq[k] = h.seq;
      seqKnown[k] = true;
    }
  }
  portEXIT_CRITICAL(&mux);
}

void addBroadcastPeer() {
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, BROADCAST, 6);
  peer.channel = 0;            // the current channel
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  if (esp_now_is_peer_exist(BROADCAST)) esp_now_mod_peer(&peer);
  else esp_now_add_peer(&peer);
}

void setChannel(uint8_t ch) {
  channel = ch;
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
}

void setup() {
  // Lights dark first.
  ctl[0] = &FastLED.addLeds<WS2811, OUT1_PIN, RGB>(leds[0], MAX_LIGHTS);
#if OUT2_STRAND
  ctl[1] = &FastLED.addLeds<WS2811, OUT2_PIN, RGB>(leds[1], MAX_LIGHTS);
#endif
#if STATUS_LED_PIN >= 0
  statusCtl = &FastLED.addLeds<WS2812, STATUS_LED_PIN, GRB>(statusLed, 1);
#endif
  FastLED.setBrightness(255);
  FastLED.clear(true);

  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.printf("EaveHub \"%s\" (%s): strand %c on GPIO %d", HUB_NAME, HUB_FW, OUT1_STRAND, OUT1_PIN);
#if OUT2_STRAND
  Serial.printf(", strand %c on GPIO %d", OUT2_STRAND, OUT2_PIN);
#endif
  Serial.println();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW did not start; restarting");
    delay(2000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onRecv);
  setChannel(1);
  addBroadcastPeer();
  Serial.print("This hub's address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Looking for the eave board...");
}

// Looks for the eave board on each channel in turn; stays once heard.
void handleLink(uint32_t now) {
  bool heard = now - lastHeardMs < SIGNAL_LOST_MS && lastHeardMs != 0;
  if (heard && !linked) {
    linked = true;
    Serial.printf("Linked to the eave board on channel %u\n", channel);
  } else if (!heard && linked) {
    linked = false;
    Serial.println("Signal lost, looking again...");
  }
  if (!linked && now - scanMs >= SCAN_STEP_MS) {
    scanMs = now;
    setChannel(channel % 13 + 1);
  }
}

// % of the eave board's frames that arrived, over about 3 seconds (first strand with data).
void updatePct(uint32_t now) {
  if (now - pctMs < 3000) return;
  pctMs = now;
  pct = 255;
  for (int k = 0; k < NUM_OUTS; k++) {
    portENTER_CRITICAL(&mux);
    uint32_t got = framesGot[k], span = seqSpan[k];
    portEXIT_CRITICAL(&mux);
    uint32_t dGot = got - pctGot0[k], dSpan = span - pctSpan0[k];
    pctGot0[k] = got;
    pctSpan0[k] = span;
    if (dSpan && pct == 255) pct = (uint8_t)min<uint32_t>(100, dGot * 100 / dSpan);
  }
  if (!linked) pct = 0;
}

void sendReport(uint32_t now) {
  if (now - reportMs < 1000 || !linked) return;
  reportMs = now;
  RadioReport r = {};
  r.magic = RADIO_MAGIC;
  r.version = RADIO_VERSION;
  r.type = RADIO_TYPE_REPORT;
  for (int k = 0; k < NUM_OUTS; k++) if (OUT_STRAND[k] >= 0) r.strandMask |= 1 << OUT_STRAND[k];
  r.rssi = (int8_t)(rssiAvg16 / 16);
  r.pct = pct;
  r.channel = channel;
  r.uptimeS = now / 1000;
  strncpy(r.name, HUB_NAME, sizeof(r.name) - 1);
  strncpy(r.fw, HUB_FW, sizeof(r.fw) - 1);
  esp_now_send(BROADCAST, (const uint8_t*)&r, sizeof(r));
}

// Copies the newest colors to the strands (with the signal-lost fade) and sends them.
void showLights(uint32_t now) {
  if (now - showMs < 10) return;   // up to 100 updates a second
  showMs = now;
  bool any = false;
  for (int k = 0; k < NUM_OUTS; k++) {
    portENTER_CRITICAL(&mux);
    uint32_t age = now - lastFrameMs[k];
    bool dirty = stagedDirty[k];
    uint16_t count = stagedCount[k];
    if (dirty) memcpy(leds[k], staged[k], count * sizeof(CRGB));
    stagedDirty[k] = false;
    portEXIT_CRITICAL(&mux);
    if (count != shownCount[k]) {
      // the strand got shorter: darken the lights past the new end once
      if (count < shownCount[k]) {
        fill_solid(leds[k] + count, shownCount[k] - count, CRGB(0, 0, 0));
        ctl[k]->setLeds(leds[k], shownCount[k]);
        FastLED.show();
      }
      shownCount[k] = count;
      ctl[k]->setLeds(leds[k], max<int>(count, 1));
    }
    if (lastFrameMs[k] && age > SIGNAL_LOST_MS) {
      // signal lost: fade out what is showing, then stay dark
      uint32_t t = age - SIGNAL_LOST_MS;
      if (t >= FADE_OUT_MS) {
        if (darkDone[k]) continue;   // already dark: nothing to send
        fill_solid(leds[k], count, CRGB(0, 0, 0));
        darkDone[k] = true;
      } else {
        portENTER_CRITICAL(&mux);
        memcpy(leds[k], staged[k], count * sizeof(CRGB));
        portEXIT_CRITICAL(&mux);
        uint8_t level = 255 - (uint8_t)(t * 255 / FADE_OUT_MS);
        for (int i = 0; i < count; i++) leds[k][i].nscale8_video(level);
      }
      any = true;
    } else {
      darkDone[k] = false;
    }
    if (dirty) any = true;
  }
  if (statusCtl) {
    CRGB c;
    int k = STATUS_LED_COPIES ? outputFor(STATUS_LED_COPIES - 'A') : -1;
    if (!linked) {
      bool blink = (now / 500) % 2;
      c = blink ? (lastHeardMs ? CRGB(60, 0, 0) : CRGB(0, 0, 60)) : CRGB(0, 0, 0);   // red = lost, blue = looking
    } else if (k >= 0 && shownCount[k]) {
      c = leds[k][0];
      c.nscale8_video(STATUS_LED_LEVEL);
    } else {
      c = CRGB(0, 12, 0);
    }
    if (c != statusLed[0]) { statusLed[0] = c; any = true; }
  }
  if (any) FastLED.show();
}

void loop() {
  uint32_t now = millis();
  handleLink(now);
  updatePct(now);
  sendReport(now);
  showLights(now);
  if (now - printMs >= 5000) {
    printMs = now;
    if (linked) Serial.printf("Linked, channel %u, signal %d dBm, %s%% of frames\n", channel, (int)(rssiAvg16 / 16),
                              pct > 100 ? "?" : String(pct).c_str());
    else Serial.printf("Looking for the eave board (channel %u)\n", channel);
  }
}
