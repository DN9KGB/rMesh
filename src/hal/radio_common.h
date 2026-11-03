#pragma once
#include <Arduino.h>
#include "mesh/frame.h"

// Shared LoRa radio driver (RME-3).
//
// The board HALs only bring the chip up (SPI, pins, begin(), modem parameters)
// and then hand it over with radioAttachSx126x()/radioAttachSx127x(). Receive,
// transmit, channel scan (LBT) and the RX statistics live here once instead of
// being copied into every board file. checkReceive()/transmitFrame() keep their
// old signatures and are implemented in radio_common.cpp.

class SX126x;
class SX1278;

// Board-specific extras around radio access.
struct RadioHooks {
    void (*lock)();            // SPI bus shared with other peripherals
    void (*unlock)();
    void (*txPath)(bool tx);   // external PA / RX-TX switch (true = TX)
    void (*afterTx)(Frame& f); // e.g. show the frame on a display
};

// Hand an initialised chip to the shared driver and start continuous RX.
void radioAttachSx126x(SX126x* radio, const RadioHooks* hooks = nullptr);
void radioAttachSx127x(SX1278* radio, const RadioHooks* hooks = nullptr);

bool checkReceive(Frame& f);
void transmitFrame(Frame& f);

extern bool txFlag;   // TX in progress
extern bool rxFlag;   // channel busy: preamble/header detected, packet incoming

// ── Listen-before-talk ──────────────────────────────────────────────────────

enum class CadResult : uint8_t { Free, Busy, Error };

// Run one CAD (channel activity detection) and return to RX. Blocks for
// about one CAD duration (a few symbols), bounded by a timeout.
CadResult radioChannelScan();

// Decide whether the next LoRa frame may go out now (lbt on: CAD).
// Returns 0 = transmit now, otherwise the backoff in ms after which to retry.
uint32_t lbtCheck();

// ── Statistics ──────────────────────────────────────────────────────────────

struct RadioStats {
    uint32_t rxOk;          // packets received with valid CRC
    uint32_t crcErr;        // packets with payload CRC error
    uint32_t hdrErr;        // LoRa header CRC errors (SX126x only)
    uint32_t preambleOnly;  // preamble detected but no packet followed (SX126x only)
    uint32_t txOk;          // transmissions started
    uint32_t txFail;        // startTransmit() errors
    uint32_t lbtFree;       // CAD: channel free
    uint32_t lbtBusy;       // CAD: channel busy -> backoff
    uint32_t lbtForced;     // sent despite busy channel after LBT_MAX_DEFER
    float    noiseFloor;    // dBm, EWMA of the RSSI while idle
    bool     noiseValid;
    uint32_t sinceMs;       // millis() of the last reset
};
extern RadioStats radioStats;
void radioResetStats();

// Sample the noise floor while idle (call from loop, rate-limited inside).
void radioSampleNoise();

// One-line summary (serial `radio`, WebUI status line).
void radioStatsLine(char* buf, size_t len);

// ── Persisted options (NVS key "radioOpts") ─────────────────────────────────

enum : uint8_t { RELAY_DELAY_SNR = 0, RELAY_DELAY_RANDOM = 1 };

struct RadioOpts {
    uint8_t version;        // layout version of this blob
    bool    lbt;            // listen-before-talk (default on)
    uint8_t relayDelayMode; // RELAY_DELAY_SNR / RELAY_DELAY_RANDOM
    int32_t freqCorrHz;     // added to the configured frequency (crystal offset)
};
extern RadioOpts radioOpts;
void loadRadioOpts();
void saveRadioOpts();

// Configured frequency plus the crystal correction, in MHz.
float radioFrequencyMHz();

// ── Frequency error of the neighbours ───────────────────────────────────────

// Median frequency error (Hz) over the available LoRa peers; false if fewer
// than three peers have a value.
bool radioPeerFreqErrorMedian(float& medianHz, uint8_t& count);

// ── Range profiles / status output ──────────────────────────────────────────

// Bandwidth/coding rate of the band preset for a frequency (433: 62.5/6, 868: 125/5).
void loraBandPreset(float freqMHz, float& bandwidthKHz, uint8_t& codingRate);

// Index of the active range profile (util/link_math.h), -1 = custom settings.
int currentLoraProfile();

// Write the profile's SF/BW/CR into settings (caller saves). False if unknown.
bool applyLoraProfile(int index);

// Serial output: profile table (`lora profile`) and radio status (`radio`).
void printLoraProfiles();
void radioPrintStatus(bool json);
