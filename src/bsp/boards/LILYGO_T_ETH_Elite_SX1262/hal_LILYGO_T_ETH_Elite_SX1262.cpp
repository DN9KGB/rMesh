#include "hal/hal.h"
#include "hal/radio_common.h"
#include "RadioLib.h"
#include "hal/settings.h"
#include "mesh/frame.h"
#include "main.h"
#include "util/helperFunctions.h"
#include "network/webFunctions.h"
#include "hal/dutycycle.h"
#include "util/logging.h"


SPIClass loraSPI(HSPI);
SX1262 radio = new Module( LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI );




void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) { logPrintf(LOG_ERROR, "LoRa", "FAILED! code %d", state); }
}

void setWiFiLED(bool value) {
    if (!board->hasWiFiLED()) return;
    digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? !value : value);
}

bool getKeyApMode() {
    if (!board->hasUserButton()) return false;
    return !digitalRead(board->pinUserButton());
}

void initHal() {
    txFlag = false;
    rxFlag = false;

    // Inputs
    if (board->hasUserButton()) {
        pinMode(board->pinUserButton(), INPUT_PULLUP);
    }

    // Initialise the dedicated LoRa SPI bus only once (separate from ETH/SD SPI).
    // Pass -1 for SS because RadioLib controls the CS pin itself.
    // Re-calling begin() on ESP32-S3 during LoRa reinit can leave the SPI
    // peripheral in a broken state, causing radio.begin() to return -2.
    static bool spiInited = false;
    if (!spiInited) {
        spiInited = true;
        loraSPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, -1);
    }

    // Only initialize the radio if a frequency has been configured
    if (!loraConfigured(settings.loraFrequency)) {
        logPrintf(LOG_WARN, "LoRa", "No frequency configured – radio disabled.");
        loraReady = false;
        return;
    }

    int beginState = RADIOLIB_ERR_CHIP_NOT_FOUND;
    for (int attempt = 1; attempt <= 3; attempt++) {
        radio.reset();
        delay(100 * attempt);
        beginState = radio.begin();
        if (beginState == RADIOLIB_ERR_NONE) break;
        logPrintf(LOG_WARN, "LoRa", "radio.begin() attempt %d/3 failed (code %d)", attempt, beginState);
    }
    if (beginState != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "radio.begin() failed (code %d) – check wiring!", beginState);
        // Allow SPI re-initialisation on next attempt — the peripheral may
        // be in a bad state after a watchdog reset or brownout.
        spiInited = false;
        loraSPI.end();
        loraReady = false;
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

    // Start receiving
    radioAttachSx126x(&radio);  // shared driver: starts RX
    loraReady = true;
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
