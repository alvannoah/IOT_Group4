// Copy to secrets.h (git-ignored) and fill in.
#pragma once

// A quote, comma or backslash in these must be escaped with a backslash for the ESP,
// which means a double backslash here, e.g. "my\\,wifi". The ESP-01 only does 2.4 GHz.
#define WIFI_SSID "YOUR_WIFI_NAME"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// LAN IP of the computer running uvicorn (`hostname -I`), not localhost.
#define BACKEND_HOST "192.168.1.173"
#define BACKEND_PORT 8000
