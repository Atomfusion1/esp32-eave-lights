// ota - wireless firmware updates, two ways:
//
// 1. Arduino IDE: the board shows up under Tools > Port as a network port
//    ("eavelights at 192.168.x.x"). Pick it and Upload as usual; the IDE asks for
//    OTA_PASSWORD (secrets.h). Uses ArduinoOTA on port 3232.
// 2. Web page: Setup > Firmware update, choose the .bin from Sketch > Export compiled
//    Binary. POST /api/firmware?password=... with the file (multipart).
//
// The new firmware goes into the other app slot (16M partition scheme has two 3 MB
// slots) and is only switched to once it is complete and verified, so a failed or
// interrupted update leaves the current firmware running. Lights go dark during an
// update and the board restarts when it's done.

bool   otaStarted = false;
String fwError;
size_t fwBytes = 0;

void lightsOffForUpdate() {
  stopLocate();
  for (int o = 0; o < NUM_OUTPUTS; o++) fill_solid(out[o], MAX_LEDS, CRGB(0, 0, 0));
  FastLED.show();
}

void startOta() {
  ArduinoOTA.setHostname(HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWORD);
  ArduinoOTA.setMdnsEnabled(false);  // wifi.ino runs mDNS and advertises the OTA port
  ArduinoOTA.onStart([]() {
    Serial.println("OTA: update from the Arduino IDE starting");
    lightsOffForUpdate();
  });
  ArduinoOTA.onEnd([]() { Serial.println("OTA: done, restarting"); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("OTA: failed (error %u)\n", e); });
}

// Starts listening once the network is up, then handles IDE uploads.
void handleOta() {
  if (!otaStarted) {
    if (!networkUp()) return;
    ArduinoOTA.begin();
    otaStarted = true;
    Serial.println("OTA: ready for wireless uploads (Arduino IDE network port or web page)");
  }
  ArduinoOTA.handle();
}

// ---- Web upload ----

// Called repeatedly while the .bin arrives.
void handleFirmwareData() {
  HTTPUpload& up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    fwError = "";
    fwBytes = 0;
    if (server.arg("password") != OTA_PASSWORD) {
      fwError = "wrong password";
      return;
    }
    Serial.printf("OTA: web update starting (%s)\n", up.filename.c_str());
    lightsOffForUpdate();
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) fwError = Update.errorString();
  } else if (up.status == UPLOAD_FILE_WRITE) {
    if (fwError.length()) return;
    if (Update.write(up.buf, up.currentSize) != up.currentSize) fwError = Update.errorString();
    fwBytes += up.currentSize;
  } else if (up.status == UPLOAD_FILE_END) {
    if (fwError.length()) {
      if (Update.isRunning()) Update.abort();
      return;
    }
    if (!Update.end(true)) fwError = Update.errorString();
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    if (Update.isRunning()) Update.abort();
    fwError = "upload interrupted";
  }
}

// Called once the whole request has arrived.
void handleFirmwareDone() {
  if (fwError.length()) {
    Serial.printf("OTA: web update failed: %s\n", fwError.c_str());
    return sendError(fwError.c_str());
  }
  Serial.printf("OTA: web update OK (%u KB), restarting\n", (unsigned)(fwBytes / 1024));
  server.send(200, "application/json", "{\"ok\":true}");
  delay(500);
  ESP.restart();
}
