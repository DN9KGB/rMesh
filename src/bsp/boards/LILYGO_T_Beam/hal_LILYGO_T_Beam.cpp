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



SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);


void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {logPrintf(LOG_ERROR, "LoRa", "FAILED! code %d", state);}
}

void setWiFiLED(bool value) {
    if (!board->hasWiFiLED()) return;
    digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? !value : value);
}

void initHal() {
    txFlag = false;
    rxFlag = false;

    //SPI init
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);

    //Inputs
    if (board->hasUserButton()) {
        pinMode(board->pinUserButton(), INPUT);  // GPIO38 has external pull-up on T-Beam
    }

    // Only initialize RF module if frequency is configured
    if (!loraConfigured(settings.loraFrequency)) {
        logPrintf(LOG_WARN, "LoRa", "Keine Frequenz konfiguriert – HF deaktiviert.");
        loraReady = false;
        return;
    }

    //Reset flags
    int state;

    //Init
    radio.reset();
    delay(100);
    int beginState = radio.begin();
    if (beginState != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "radio.begin() failed (code %d)", beginState);
        loraReady = false;
        return;
    }
    printState(radio.setSyncWord(settings.loraSyncWord));
    printState(radio.setFrequency(radioFrequencyMHz()));
    printState(radio.setOutputPower(settings.loraOutputPower));
    printState(radio.setBandwidth(settings.loraBandwidth));
    printState(radio.setCodingRate(settings.loraCodingRate));
    printState(radio.setSpreadingFactor(settings.loraSpreadingFactor));
    printState(radio.setPreambleLength(settings.loraPreambleLength));
    printState(radio.setCRC(true));
    printState(radio.setCurrentLimit(240));
    printState(radio.setGain(0));

    //Enable RX
    radioAttachSx127x(&radio);  // shared driver: starts RX
    loraReady = true;


}


bool getKeyApMode() {
    if (!board->hasUserButton()) return false;
    return !digitalRead(board->pinUserButton());
}

/**
 * @brief Start a continuous tune carrier (#54).
 *
 * The SX127x has no CW command in LoRa mode, so switch to FSK with zero
 * frequency deviation and enter direct-mode TX — an unmodulated carrier at
 * the configured frequency/power. The main loop ends the carrier after
 * TUNE_DURATION by calling initHal(), which restores the LoRa modem.
 */
void tuneStart() {
    if (!loraReady) return;
    radio.standby();
    radio.beginFSK(radioFrequencyMHz(), 4.8f, 0.0f, 125.0f, settings.loraOutputPower, 16, false);
    radio.transmitDirect();
}
