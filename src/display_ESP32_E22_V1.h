#pragma once

#ifdef ESP32_E22_V1

/**
 * @brief SSD1306 0.96" OLED status display for the ESP32 E22 Multimodul (Rentner Gang).
 *
 * Optional on-board I²C display (LCD1). Shown only if detected on the bus.
 */

bool initStatusDisplay();
void updateStatusDisplay();
void enableStatusDisplay();
void disableStatusDisplay();
bool hasStatusDisplay();
void onStatusDisplayMessage(const char* srcCall, const char* text, const char* dstGroup, const char* dstCall);

#endif // ESP32_E22_V1
