#include "hal/hal.h"
#include "RadioLib.h"
#include "hal/settings.h"
#include "mesh/frame.h"
#include "main.h"
#include "util/helperFunctions.h"
#include "network/webFunctions.h"
#include "util/logging.h"

void setWiFiLED(bool value) {
    if (!board->hasWiFiLED()) return;
    digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? !value : value);
}

bool getKeyApMode() {
    if (!board->hasUserButton()) return false;
    return !digitalRead(board->pinUserButton());
}

void initHal() {
   //Outputs
    if (board->hasWiFiLED()) {
        pinMode(board->pinWiFiLED(), OUTPUT);
        digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? HIGH : LOW);
    }

    //Inputs
    if (board->hasUserButton()) {
        pinMode(board->pinUserButton(), INPUT);
    }

    loraReady = false;  // No RF module present
}

// checkReceive()/transmitFrame() come from hal/radio_common.cpp: without an
// attached radio they receive nothing and transmit nothing.

/** Tune carrier (#54): no RF module present — nothing to do. */
void tuneStart() {
}
