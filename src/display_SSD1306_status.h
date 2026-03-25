#pragma once

/**
 * @brief Small SSD1306 OLED status display for boards with 128x64 OLED.
 *
 * Shows callsign, battery, WiFi mode, IP, SSID and last received message.
 * Controlled via boot button (short press) and oledEnabled setting (persisted).
 */

/// Probe I2C for SSD1306 and initialise U8g2 if found.
/// If oledEnabled is already true, the display turns on immediately.
/// Returns true if display hardware was detected.
bool initStatusDisplay();

/// Redraw the status screen (call, battery, mode, IP, SSID, last message).
void updateStatusDisplay();

/// Turn display on, draw content, set oledEnabled = true and persist.
void enableStatusDisplay();

/// Turn display off (power save), set oledEnabled = false and persist.
void disableStatusDisplay();

/// Returns true if a display was detected during init.
bool hasStatusDisplay();

/// Called from main.cpp when a new TEXT_MESSAGE arrives.
/// Stores message if dstGroup matches oledDisplayGroup.
void onStatusDisplayMessage(const char* srcCall, const char* text, const char* dstGroup, const char* dstCall);
