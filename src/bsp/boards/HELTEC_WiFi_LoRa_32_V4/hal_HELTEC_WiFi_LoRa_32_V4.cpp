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



SX1262 radio = new Module( LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY );




void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {logPrintf(LOG_ERROR, "LoRa", "FAILED! code %d", state);}
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

    //SPI init
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

    //Outputs
    if (board->hasWiFiLED()) { pinMode(board->pinWiFiLED(), OUTPUT); digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? HIGH : LOW); }
    pinMode( PIN_PA_CPS, OUTPUT);
    pinMode( PIN_PA_CSD, OUTPUT);
    pinMode( PIN_VFEM, OUTPUT);
    digitalWrite(PIN_VFEM, 1);      //PA voltage on
    digitalWrite(PIN_PA_CSD, 1);
    digitalWrite(PIN_PA_CPS, 1);

    //Inputs
    if (board->hasUserButton()) { pinMode(board->pinUserButton(), INPUT); }

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
    printState(radio.setDio2AsRfSwitch(true));
    printState(radio.setSyncWord(settings.loraSyncWord));
    printState(radio.setFrequency(radioFrequencyMHz()));
    printState(radio.setOutputPower(settings.loraOutputPower));
    printState(radio.setBandwidth(settings.loraBandwidth));
    printState(radio.setCodingRate(settings.loraCodingRate));
    printState(radio.setSpreadingFactor(settings.loraSpreadingFactor));
    printState(radio.setPreambleLength(settings.loraPreambleLength));
    printState(radio.setCRC(true));
    printState(radio.setCurrentLimit(140)) ;
    printState(radio.setRxBoostedGainMode(true));

    //Enable RX
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
