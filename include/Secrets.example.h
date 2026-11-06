#pragma once
// Copy to include/Secrets.h (git-ignored) and fill in. Without Secrets.h the gateway builds with the uplink off and only prints to the serial port.
#define TW_WIFI_SSID "your-hotspot-name"
#define TW_WIFI_PASSWORD "your-hotspot-password"
#define TW_BACKEND_URL "https://terra-watch-server.vercel.app"   // no trailing slash
#define TW_GATEWAY_ID "gw1"
#define TW_GATEWAY_SECRET "the secret that goes with this gateway id in STATION_GATEWAY_KEYS"
