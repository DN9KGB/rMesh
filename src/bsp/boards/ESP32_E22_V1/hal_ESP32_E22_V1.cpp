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



SX1268 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);


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
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);

    //Outputs
    if (board->hasWiFiLED()) {
        pinMode(board->pinWiFiLED(), OUTPUT);
        digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? HIGH : LOW);
    }
    pinMode(LORA_TX_ENA, OUTPUT);
    digitalWrite(LORA_TX_ENA, 0);
    pinMode(LORA_RX_ENA, OUTPUT);
    digitalWrite(LORA_RX_ENA, 1);

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
//    printState(radio.setDio2AsRfSwitch(true));
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
    // External PA/LNA switch: RX_ENA/TX_ENA follow the TX state
    static const RadioHooks hooks = {nullptr, nullptr, [](bool tx) {
        digitalWrite(LORA_RX_ENA, tx ? 0 : 1);
        digitalWrite(LORA_TX_ENA, tx ? 1 : 0);
    }, nullptr};
    radioAttachSx126x(&radio, &hooks);  // shared driver: starts RX

    //Register test PEER
    //Peer p;
    //p.lastRX = 0xFFFFFFFF;
    //strncpy(p.call, "DB0LUS", sizeof(p.call)-1);  //DB0LUS in p.call
    //p.available = true;
    //peerList.push_back(p);

}

/**
 * @brief Start a continuous tune carrier (#54).
 *
 * Enables the external PA (TX_ENA) and puts the SX1268 into unmodulated
 * continuous-wave TX. The main loop ends the carrier after TUNE_DURATION by
 * calling initHal(), which restores the RX switch state and RX mode.
 */
void tuneStart() {
    if (!loraReady) return;
    digitalWrite(LORA_RX_ENA, 0);
    digitalWrite(LORA_TX_ENA, 1);
    radio.standby();
    radio.transmitDirect();
}
