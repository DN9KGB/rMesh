#pragma once

#include <Arduino.h>

// Detects a node whose LittleFS partition carries no WebUI/help files —
// typically littlefs.bin flashed to a wrong address (e.g. the outdated
// 0x290000), after which the firmware formats the empty partition and runs
// without WebUI and console help.

/** Short console help used when /help.txt is missing. */
extern const char EMERGENCY_HELP[];

#ifndef NRF52_PLATFORM
/** True if the boot-time mount failed and the partition had to be formatted. */
extern bool fsWasFormatted;

/** True if /index.html or /index.html.gz exists on LittleFS. */
bool webUiFilesPresent();

/** Flash address of the LittleFS (data/spiffs) partition, 0 if not found. */
uint32_t fsPartitionOffset();

/** Logs the "filesystem empty" hint with the correct littlefs.bin offset. */
void logFsMissingHint();
#endif
