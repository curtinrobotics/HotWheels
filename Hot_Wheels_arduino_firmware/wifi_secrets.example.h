#pragma once

// Copy this file to `wifi_secrets.h` (same folder) and fill in your values.
// `wifi_secrets.h` is ignored by git via .gitignore.

// WiFi credentials (2.4GHz recommended for many ESP32 setups)
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// How the board will appear in Arduino IDE -> Tools -> Port -> Network ports
#define OTA_HOSTNAME "HotWheels_RC"

// Optional: require a password for OTA uploads (leave blank for none)
#define OTA_PASSWORD ""

// Optional: stream logs over WiFi (view with `nc <board-ip> <port>`)
// Set to 0 to disable.
#define LOG_TCP_PORT 2323
