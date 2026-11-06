// HTTP GET over an ESP-01 running the stock AT firmware.
#pragma once

#include <stddef.h>

const int HTTP_FAILED = -1;

void espBegin();

// Resets the module and joins WiFi. Blocks for up to about 30 seconds.
bool espConnectWifi();

// Sends a GET to the backend and copies the reply body into `body`. Returns the HTTP
// status, or HTTP_FAILED if no complete reply arrived. Blocks for up to about 15 seconds.
int espGet(const char *path, char *body, size_t bodySize);
