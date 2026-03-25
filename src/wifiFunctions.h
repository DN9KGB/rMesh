#pragma once

void wifiInit();
void showWiFiStatus();
void checkForUpdates(bool force = false, uint8_t forceChannel = 0);

// Flag: scan triggered for reconnect to pick best network from list
extern bool pendingReconnectScan;
