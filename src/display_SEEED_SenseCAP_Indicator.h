#pragma once

#ifdef SEEED_SENSECAP_INDICATOR

#include "frame.h"

/*
 * display_SEEED_SenseCAP_Indicator.h
 *
 * UI für den Seeed SenseCAP Indicator: ST7701S 480×480 RGB-Display.
 * Gleiche UI-Struktur wie der T-LoraPager, angepasst für quadratisches Display.
 *
 * Layout (480 × 480):
 *   ┌─────────────────────────────────────────┐  y=0
 *   │  Header (Rufzeichen | Status | Uhrzeit) │  18 px
 *   ├─────────────────────────────────────────┤  y=18
 *   │  Gruppen-Tabs                           │  18 px
 *   ├─────────────────────────────────────────┤  y=36
 *   │                                         │
 *   │  Nachrichtenliste  (480 × 412 px)       │
 *   │                                         │
 *   ├─────────────────────────────────────────┤  y=448
 *   │  Status-Bar        (480 ×  32 px)       │
 *   └─────────────────────────────────────────┘  y=480
 */

void initDisplay();
void displayUpdateLoop();
void displayOnNewMessage(const char* srcCall, const char* text, const char* dstGroup = "", const char* dstCall = "");
void displayTxFrame(const char* dstCall, const char* text);
void displayMonitorFrame(const Frame& f);

// PCA9535-Zugriff für LoRa-HAL (Bits 0-3 = LORA CS/RST/BUSY/DIO1)
void     pca9535_write_bit(int bit, bool value);
bool     pca9535_read_bit(int bit);

#endif // SEEED_SENSECAP_INDICATOR
