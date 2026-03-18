#pragma once
#include <ESPAsyncWebServer.h>
#include "auth.h"

extern AsyncWebSocket ws;

void startWebServer();
void wsBroadcast(const char* buf, size_t len);
