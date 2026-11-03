#include <SPI.h>
#include "hal/hal.h"
#include "hal/radio_common.h"
#include "RadioLib.h"
#include "hal/settings.h"
#include "mesh/frame.h"
#include "main.h"
#include "util/helperFunctions.h"
#include "hal/dutycycle.h"
#include "util/logging.h"

// ── LoRa Radio (SX1262 on dedicated SPI bus) ────────────────────────────────

// LoRa uses dedicated SPIM3 (default SPI/SPIM2 is reserved for E-Paper display)
SPIClass loraSPI(NRF_SPIM3, LORA_MISO, LORA_SCK, LORA_MOSI);

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY, loraSPI);


static void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "FAILED! code %d", state);
    }
}

void setWiFiLED(bool value) {
    if (!board->hasWiFiLED()) return;
    digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? !value : value);
}

bool getKeyApMode() {
    if (!board->hasUserButton()) return false;
    return !digitalRead(board->pinUserButton());
}

float getBatteryVoltage() {
    if (!board->hasBatteryMonitor()) return 0.0f;
    analogReference(AR_INTERNAL_3_0);   // 3.0V reference
    analogReadResolution(12);           // 12-bit ADC
    delay(1);
    int raw = analogRead(board->pinBatteryADC());
    // Convert: ADC value -> voltage with divider compensation
    return (raw / 4095.0f) * 3.0f * board->batteryVoltageMultiplier();
}

void initHal() {
    txFlag = false;
    rxFlag = false;

    // LED outputs (WiFi LED = green LED on T-Echo)
    if (board->hasWiFiLED()) {
        pinMode(board->pinWiFiLED(), OUTPUT);
        digitalWrite(board->pinWiFiLED(), board->wiFiLEDActiveLow() ? HIGH : LOW);
    }
    pinMode(PIN_LED_BLUE, OUTPUT);
    digitalWrite(PIN_LED_BLUE, LOW);

    // Button input (active low with internal pull-up)
    if (board->hasUserButton()) {
        pinMode(board->pinUserButton(), INPUT_PULLUP);
    }

    // Power down the L76K GPS: rMesh does not use it, and leaving it in its
    // power-on default draws ~20-30 mA continuously on this battery-only board.
    // GPS_EN is active-high (HIGH = powered); drive LOW to keep it off.
    pinMode(GPS_EN_PIN, OUTPUT);
    digitalWrite(GPS_EN_PIN, LOW);

    // Start LoRa SPI bus (SPIM3, separate from display on SPIM2)
    loraSPI.begin();

    // Check if LoRa frequency is configured
    if (!loraConfigured(settings.loraFrequency)) {
        logPrintf(LOG_WARN, "LoRa", "No frequency configured - RF disabled.");
        loraReady = false;
        return;
    }

    // Initialize SX1262 with TCXO voltage (1.8V) included so RadioLib
    // calibrates the image rejection filter for the correct frequency band.
    int8_t txPower = settings.loraOutputPower;
    if (txPower > 22) txPower = 22;  // SX1262 max is +22 dBm

    int state = radio.begin(
        radioFrequencyMHz(),
        settings.loraBandwidth,
        settings.loraSpreadingFactor,
        settings.loraCodingRate,
        settings.loraSyncWord,
        txPower,
        settings.loraPreambleLength,
        1.8,    // TCXO voltage
        false   // use DC-DC regulator (not LDO)
    );
    if (state != RADIOLIB_ERR_NONE) {
        logPrintf(LOG_ERROR, "LoRa", "SX1262 init failed (code %d)", state);
        loraReady = false;
        return;
    }

    printState(radio.setDio2AsRfSwitch(true));
    printState(radio.setCRC(true));
    printState(radio.setCurrentLimit(140));
    printState(radio.setRxBoostedGainMode(true));

    // Start continuous receive
    radioAttachSx126x(&radio);  // shared driver: starts RX
    loraReady = true;
    logPrintf(LOG_INFO, "LoRa", "Ready: %.3f MHz SF%d BW%.1f",
              settings.loraFrequency, settings.loraSpreadingFactor,
              settings.loraBandwidth);

    // Brief LED flash to confirm radio init
    if (board->hasWiFiLED()) {
        digitalWrite(board->pinWiFiLED(), HIGH);
        delay(100);
        digitalWrite(board->pinWiFiLED(), LOW);
    }
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
