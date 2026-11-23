// Copy this file to secrets.h (same folder) and fill in your details.
// secrets.h is in .gitignore so your password is never committed.

#pragma once

// Home Wi-Fi (2.4 GHz only). Leave WIFI_SSID as "" to always use the hotspot.
#define WIFI_SSID     "your-network-name"
#define WIFI_PASSWORD "your-wifi-password"

// Fallback hotspot, used when the home network can't be reached.
// Password must be at least 8 characters.
#define AP_SSID       "EaveLights"
#define AP_PASSWORD   "change-me-123"

// Password for wireless firmware updates (Arduino IDE network port and web page).
#define OTA_PASSWORD  "change-me-too"
