/*
 * hal_LILYGO_T_LoraPager.cpp
 *
 * Hardware abstraction layer for the LILYGO T-LoraPager (ESP32-S3 + SX1262).
 *
 * SPI bus coordination:
 *   - instance.begin() initializes the entire SPI bus and IO expander.
 *   - All RadioLib accesses are protected with instance.lockSPI() / unlockSPI()
 *     so that LoRa and display can share the bus.
 *   - LovyanGFX uses the same lock (use_lock=true in bus configuration).
 */

#include "hal/hal.h"
#include "hal/radio_common.h"
#include "display/display_LILYGO_T_LoraPager.h"
#include "RadioLib.h"
#include "hal/settings.h"
#include "mesh/frame.h"
#include "main.h"
#include "util/helperFunctions.h"
#include "network/webFunctions.h"
#include "hal/dutycycle.h"
#include "util/logging.h"

// LilyGoLib instance (defined in LilyGo_LoRa_Pager.cpp)
#include <LilyGoLib.h>
#include <SD.h>

// radio is defined by LilyGoLib (via ARDUINO_LILYGO_LORA_SX1262 in LilyGo_LoRa_Pager.cpp)


// ─── SD card ──────────────────────────────────────────────────────────────

static bool sdMounted = false;

bool pagerSdAvailable() { return sdMounted; }

// Called from the main Arduino loop — synchronous write is safe here.
// A FreeRTOS task was tried previously but it held instance.lockSPI() across
// the full FAT32 open/write/close, which blocked display and input for hundreds
// of milliseconds and made the device appear to hang.
void pagerAddMessageToSD(const char* json, size_t len) {
    if (!sdMounted) return;
    instance.lockSPI();
    File f = SD.open("/messages.json", FILE_APPEND);
    if (f) {
        f.write((const uint8_t*)json, len);
        f.print('\n');
        f.close();
    }
    instance.unlockSPI();
}

// ─── Helper functions ────────────────────────────────────────────────────

static void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "FAILED code %d", state);
    }
}

void setWiFiLED(bool value) {
    // T-LoraPager has no dedicated WiFi LED
    (void)value;
}

bool getKeyApMode() {
    if (!board->hasUserButton()) return false;
    return !digitalRead(board->pinUserButton());
}

// ─── Hardware init ───────────────────────────────────────────────────────

void initHal() {
    txFlag  = false;
    rxFlag  = false;

    if (board->hasUserButton()) {
        pinMode(board->pinUserButton(), INPUT);
    }

    // Display + Keyboard init (calls instance.begin() internally,
    // which sets up SPI bus, XL9555, TCA8418 keyboard, and also tries installSD()).
    initDisplay();

    // SD state is determined by instance.begin() inside initDisplay()
    sdMounted = (SD.cardType() != CARD_NONE);
    logPrintf(LOG_INFO, "HAL", "%s", sdMounted ? "SD card mounted" : "SD no card detected");

    // Only initialize LoRa if frequency is configured
    if (!loraConfigured(settings.loraFrequency)) {
        logPrintf(LOG_WARN, "LoRa", "Keine Frequenz konfiguriert – HF deaktiviert.");
        loraReady = false;
        return;
    }

    // LoRa init on already configured SPI bus, with SPI lock
    instance.lockSPI();

    radio.reset();
    delay(100);
    int beginState = radio.begin();
    if (beginState != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "radio.begin() failed (code %d)", beginState);
        loraReady = false;
        instance.unlockSPI();
        return;
    }
    printState(radio.setDio2AsRfSwitch(true));
    printState(radio.setSyncWord(settings.loraSyncWord));
    printState(radio.setFrequency(radioFrequencyMHz()));
    printState(radio.setOutputPower(settings.loraOutputPower));
    printState(radio.setBandwidth(settings.loraBandwidth));
    printState(radio.setCodingRate(settings.loraCodingRate));
    printState(radio.setSpreadingFactor(settings.loraSpreadingFactor));
    printState(radio.setPreambleLength(settings.loraPreambleLength));
    printState(radio.setCRC(true));
    printState(radio.setCurrentLimit(140));
    printState(radio.setRxBoostedGainMode(true));
    // The radio shares its SPI bus with display/SD/NFC: guard every access
    static const RadioHooks hooks = {
        [] { instance.lockSPI(); },
        [] { instance.unlockSPI(); },
        nullptr,
        [](Frame& f) { displayMonitorFrame(f); },
    };

    instance.unlockSPI();
    radioAttachSx126x(&radio, &hooks);  // shared driver: starts RX (takes the lock itself)

    loraReady = true;
    logPrintf(LOG_INFO, "LoRa", "SX1262 ready.");
}

/**
 * @brief Start a continuous tune carrier (#54).
 *
 * Puts the radio into unmodulated continuous-wave TX at the configured
 * frequency/power. The main loop ends the carrier after TUNE_DURATION by
 * calling initHal(), which fully re-initialises the radio into RX mode.
 */
void tuneStart() {
    if (!loraReady) return;
    radio.standby();
    radio.transmitDirect();
}
