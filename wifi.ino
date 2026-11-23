// wifi — joins the home network; if that fails, starts its own hotspot.
//
// - Tries WIFI_SSID for WIFI_CONNECT_TIMEOUT_MS after boot (or after losing the connection).
// - If still not connected, starts the AP_SSID hotspot (web page at http://192.168.4.1).
// - While the hotspot is up, retries the home network every WIFI_RETRY_MS, but only when
//   nobody is connected to the hotspot (a retry can briefly disturb hotspot clients).
// - Once the home network connects, the hotspot is turned off.
// - The lights keep running the whole time; nothing here blocks loop().

const uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
const uint32_t WIFI_RETRY_MS           = 120000;

uint32_t wifiAttemptMs  = 0;
bool     wifiConnected  = false;
bool     hotspotActive  = false;
bool     mdnsStarted    = false;

void startWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  WiFi.setAutoReconnect(true);
  wifiAttemptMs = millis();

  if (strlen(WIFI_SSID) == 0) {
    Serial.println("Wi-Fi: no home network set in secrets.h");
    startHotspot();
    return;
  }
  Serial.printf("Wi-Fi: connecting to \"%s\"\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void startHotspot() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  WiFi.setAutoReconnect(false);  // retries are done by hand, see handleWifi()
  hotspotActive = true;
  wifiAttemptMs = millis();
  Serial.printf("Wi-Fi: hotspot \"%s\" started, open http://%s\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());
  startMdns();
}

void stopHotspot() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  hotspotActive = false;
  Serial.println("Wi-Fi: hotspot stopped");
}

// True once the board is reachable: on the home network or running its hotspot.
bool networkUp() {
  return WiFi.status() == WL_CONNECTED || hotspotActive;
}

void startMdns() {
  if (mdnsStarted) return;
  if (MDNS.begin(HOSTNAME)) {
    MDNS.addService("http", "tcp", 80);
    MDNS.enableArduino(3232, true);   // lets the Arduino IDE list the board as a network port
    mdnsStarted = true;
  }
}

void handleWifi() {
  bool connectedNow = (WiFi.status() == WL_CONNECTED);
  uint32_t now = millis();

  if (connectedNow && !wifiConnected) {
    wifiConnected = true;
    Serial.printf("Wi-Fi: connected to \"%s\", IP %s -> http://%s.local\n",
                  WIFI_SSID, WiFi.localIP().toString().c_str(), HOSTNAME);
    startMdns();
    if (hotspotActive) stopHotspot();
  } else if (!connectedNow && wifiConnected) {
    wifiConnected = false;
    wifiAttemptMs = now;
    Serial.println("Wi-Fi: connection lost, retrying");
  }

  if (wifiConnected || strlen(WIFI_SSID) == 0) return;

  if (!hotspotActive && now - wifiAttemptMs > WIFI_CONNECT_TIMEOUT_MS) {
    Serial.println("Wi-Fi: could not connect to home network");
    startHotspot();
  } else if (hotspotActive && now - wifiAttemptMs > WIFI_RETRY_MS &&
             WiFi.softAPgetStationNum() == 0) {
    wifiAttemptMs = now;
    Serial.println("Wi-Fi: retrying home network");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}
