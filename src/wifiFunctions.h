#pragma once

#include <WiFi.h>

void wifiInit();
void showWiFiStatus();
void checkForUpdates(bool force = false, uint8_t forceChannel = 0);
void onWiFiScanDone(WiFiEvent_t event, WiFiEventInfo_t info);

// Flag: scan triggered for reconnect to pick best network from list
extern bool pendingReconnectScan;
