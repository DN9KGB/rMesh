#include "hal/radio_common.h"
#include <RadioLib.h>
#include "hal/settings.h"
#include "hal/dutycycle.h"
#include "main.h"
#include "mesh/peer.h"
#include "util/helperFunctions.h"
#include "util/link_math.h"
#include "util/logging.h"
#include "util/lora_math.h"
#include <ArduinoJson.h>

bool txFlag = false;
bool rxFlag = false;

RadioStats radioStats = {};
RadioOpts  radioOpts  = {1, true, RELAY_DELAY_SNR, 0};

static SX126x*    r126 = nullptr;
static SX1278*    r127 = nullptr;
static RadioHooks hooks;

// Millis since a preamble was detected without a following header (SX126x)
static uint32_t preambleSince = 0;

// SX126x RX IRQs: RadioLib defaults plus PREAMBLE_DETECTED, so the channel
// counts as busy from the first preamble symbols on, not only after the header.
static const RadioLibIrqFlags_t SX126X_RX_FLAGS =
    RADIOLIB_IRQ_RX_DEFAULT_FLAGS | (1UL << RADIOLIB_IRQ_PREAMBLE_DETECTED);

// SX127x RegModemStat: bit1 signal synchronised, bit3 header info valid
static const uint8_t SX127X_MODEM_BUSY = 0x02 | 0x08;

static inline void lock()   { if (hooks.lock) hooks.lock(); }
static inline void unlock() { if (hooks.unlock) hooks.unlock(); }

// Restart continuous RX (caller holds the lock)
static void restartRx() {
    if (r126) r126->startReceive(RADIOLIB_SX126X_RX_TIMEOUT_INF, SX126X_RX_FLAGS, RADIOLIB_IRQ_RX_DEFAULT_MASK, 0);
    else if (r127) r127->startReceive();
    preambleSince = 0;
}

void radioAttachSx126x(SX126x* radio, const RadioHooks* h) {
    r126 = radio;
    r127 = nullptr;
    hooks = h ? *h : RadioHooks();
    txFlag = false;
    rxFlag = false;
    lock();
    restartRx();
    unlock();
}

void radioAttachSx127x(SX1278* radio, const RadioHooks* h) {
    r127 = radio;
    r126 = nullptr;
    hooks = h ? *h : RadioHooks();
    txFlag = false;
    rxFlag = false;
    lock();
    restartRx();
    unlock();
}

static void setRxFlag(bool busy) {
    if (busy != rxFlag) {
        rxFlag = busy;
        statusTimer = 0;
    }
}

static void onTxDone() {
    if (hooks.txPath) hooks.txPath(false);
    lock();
    restartRx();
    unlock();
    txFlag = false;
    statusTimer = 0;
}

// Read the received packet, restart RX, fill f. Returns true for a valid packet.
static bool readPacket(PhysicalLayer* phy, Frame& f) {
    uint8_t rxBuffer[256];
    lock();
    size_t rxBufferLength = phy->getPacketLength();
    int16_t state = phy->readData(rxBuffer, rxBufferLength);
    // Read RSSI/SNR BEFORE startReceive(), as startReceive() can reset the registers
    float rxRssi     = phy->getRSSI();
    float rxSnr      = phy->getSNR();
    float rxFrqError = r126 ? r126->getFrequencyError() : r127->getFrequencyError();
    restartRx();
    unlock();

    if (state == RADIOLIB_ERR_CRC_MISMATCH) { radioStats.crcErr++; return false; }
    if (state != RADIOLIB_ERR_NONE) return false;
    radioStats.rxOk++;
    f.importBinary(rxBuffer, rxBufferLength);
    f.tx        = false;
    f.timestamp = time(NULL);
    f.rssi      = rxRssi;
    f.snr       = rxSnr;
    f.frqError  = rxFrqError;
    f.port      = 0;
    return true;
}

static bool checkReceiveSx126x(Frame& f) {
    lock();
    uint32_t irq = r126->getIrqFlags();
    // A header CRC error leaves the preamble flag latched: count and clear it
    if (irq & RADIOLIB_SX126X_IRQ_HEADER_ERR) {
        radioStats.hdrErr++;
        r126->clearIrqFlags(RADIOLIB_SX126X_IRQ_HEADER_ERR | RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED);
        irq &= ~(uint32_t)(RADIOLIB_SX126X_IRQ_HEADER_ERR | RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED);
        preambleSince = 0;
    }
    // A preamble without a header within the expected time was a false/aborted
    // detection; clear it so the channel does not stay "busy" until the watchdog
    if ((irq & RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED) && !(irq & (RADIOLIB_SX126X_IRQ_HEADER_VALID | RADIOLIB_SX126X_IRQ_RX_DONE))) {
        if (preambleSince == 0) {
            preambleSince = millis();
        } else if ((millis() - preambleSince) > preambleTimeoutMs(settings.loraSpreadingFactor, settings.loraBandwidth, settings.loraPreambleLength)) {
            radioStats.preambleOnly++;
            r126->clearIrqFlags(RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED);
            irq &= ~(uint32_t)RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED;
            preambleSince = 0;
        }
    } else {
        preambleSince = 0;
    }
    unlock();

    setRxFlag((irq & (RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED | RADIOLIB_SX126X_IRQ_HEADER_VALID)) != 0);

    if (irq & RADIOLIB_SX126X_IRQ_TX_DONE) onTxDone();
    if (irq & RADIOLIB_SX126X_IRQ_RX_DONE) {
        bool ok = readPacket(r126, f);
        setRxFlag(false);
        return ok;
    }
    return false;
}

static bool checkReceiveSx127x(Frame& f) {
    lock();
    uint16_t irq = r127->getIRQFlags();
    uint8_t modem = r127->getModemStatus();
    unlock();

    setRxFlag((irq & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_VALID_HEADER) || (modem & SX127X_MODEM_BUSY));

    if (irq & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_TX_DONE) onTxDone();
    if (irq & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_RX_DONE) {
        bool ok = readPacket(r127, f);
        setRxFlag(false);
        return ok;
    }
    return false;
}

bool checkReceive(Frame& f) {
    if (!loraReady) return false;
    if (r126) return checkReceiveSx126x(f);
    if (r127) return checkReceiveSx127x(f);
    return false;
}

void transmitFrame(Frame& f) {
    if (!loraReady || (!r126 && !r127)) return;
    PhysicalLayer* phy = r126 ? (PhysicalLayer*)r126 : (PhysicalLayer*)r127;

    uint8_t txBuffer[255];
    size_t txBufferLength;

    //Populate frame
    statusTimer = 0;
    strncpy(f.nodeCall, settings.mycall, sizeof(f.nodeCall));
    f.tx = true;
    f.timestamp = time(NULL);
    f.port = 0;

    if (strlen(f.nodeCall) == 0) {return;}
    txBufferLength = f.exportBinary(txBuffer, sizeof(txBuffer));

    // Duty cycle check for public band (10% in 60s)
    if (isPublicBand(settings.loraFrequency)) {
        lock();
        uint32_t toa = (uint32_t)(phy->getTimeOnAir(txBufferLength) / 1000);
        unlock();
        if (!dutyCycleAllowed(toa)) {
            logPrintf(LOG_WARN, "LoRa", "Duty cycle limit reached, TX skipped.");
            return;
        }
    }

    if (hooks.txPath) hooks.txPath(true);
    txFlag = true;
    lock();
    int16_t state = phy->startTransmit(txBuffer, txBufferLength);
    if (state != RADIOLIB_ERR_NONE) restartRx();
    unlock();
    if (state != RADIOLIB_ERR_NONE) {
        // No TX_DONE will follow: release the flag now instead of waiting for the watchdog
        logPrintf(LOG_ERROR, "LoRa", "startTransmit failed (code %d)", state);
        radioStats.txFail++;
        txFlag = false;
        if (hooks.txPath) hooks.txPath(false);
        return;
    }
    radioStats.txOk++;

    //Monitor frame
    f.monitorJSON();
    if (hooks.afterTx) hooks.afterTx(f);
}

// ── Listen-before-talk ──────────────────────────────────────────────────────

CadResult radioChannelScan() {
    if (!loraReady || (!r126 && !r127)) return CadResult::Error;
    // CAD timeout: generous bound of 16 symbols (a CAD takes 2-4)
    uint32_t timeout = (uint32_t)(16.0f * symbolTimeMs(settings.loraSpreadingFactor, settings.loraBandwidth)) + 10;
    bool done = false, detected = false;

    if (r126) {
        lock();
        // Never abort a reception that is already under way
        uint32_t irq = r126->getIrqFlags();
        if (irq & (RADIOLIB_SX126X_IRQ_RX_DONE | RADIOLIB_SX126X_IRQ_PREAMBLE_DETECTED | RADIOLIB_SX126X_IRQ_HEADER_VALID)) {
            unlock();
            return CadResult::Busy;
        }
        int16_t state = r126->startChannelScan();
        unlock();
        if (state != RADIOLIB_ERR_NONE) {
            lock(); restartRx(); unlock();
            return CadResult::Error;
        }
        uint32_t t0 = millis();
        while ((millis() - t0) < timeout) {
            lock();
            irq = r126->getIrqFlags();
            unlock();
            if (irq & RADIOLIB_SX126X_IRQ_CAD_DONE) {
                done = true;
                detected = (irq & RADIOLIB_SX126X_IRQ_CAD_DETECTED) != 0;
                break;
            }
            delay(1);
        }
    } else {
        lock();
        uint16_t irq = r127->getIRQFlags();
        if ((irq & (RADIOLIB_SX127X_CLEAR_IRQ_FLAG_RX_DONE | RADIOLIB_SX127X_CLEAR_IRQ_FLAG_VALID_HEADER)) ||
            (r127->getModemStatus() & SX127X_MODEM_BUSY)) {
            unlock();
            return CadResult::Busy;
        }
        int16_t state = r127->startChannelScan();
        unlock();
        if (state != RADIOLIB_ERR_NONE) {
            lock(); restartRx(); unlock();
            return CadResult::Error;
        }
        uint32_t t0 = millis();
        while ((millis() - t0) < timeout) {
            lock();
            irq = r127->getIRQFlags();
            unlock();
            if (irq & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_CAD_DONE) {
                done = true;
                detected = (irq & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_CAD_DETECTED) != 0;
                break;
            }
            delay(1);
        }
    }

    lock();
    restartRx();
    unlock();
    if (!done) return CadResult::Error;
    return detected ? CadResult::Busy : CadResult::Free;
}

uint32_t lbtCheck() {
    static uint8_t busyStreak = 0;
    if (!radioOpts.lbt) return 0;
    CadResult res = radioChannelScan();
    if (res != CadResult::Busy) {
        // A failed CAD must not block TX: behave like LBT off
        if (res == CadResult::Free) radioStats.lbtFree++;
        busyStreak = 0;
        return 0;
    }
    if (busyStreak >= LBT_MAX_DEFER) {
        radioStats.lbtForced++;
        busyStreak = 0;
        logPrintf(LOG_WARN, "LoRa", "LBT: channel busy %u times in a row, transmitting anyway", (unsigned)LBT_MAX_DEFER);
        return 0;
    }
    busyStreak++;
    radioStats.lbtBusy++;
    return lbtBackoffMs(lbtSlotMs(settings.loraSpreadingFactor, settings.loraBandwidth),
                        busyStreak, (uint32_t)random(0x7FFFFFFF));
}

// ── Statistics ──────────────────────────────────────────────────────────────

void radioResetStats() {
    radioStats = RadioStats();
    radioStats.sinceMs = millis();
}

void radioSampleNoise() {
    static uint32_t last = 0;
    if ((millis() - last) < 5000) return;
    last = millis();
    if (!loraReady || txFlag || rxFlag || (!r126 && !r127)) return;
    lock();
    float rssi = r126 ? r126->getRSSI(false) : r127->getRSSI(false, true);
    unlock();
    if (rssi > -10.0f || rssi < -160.0f) return;   // implausible reading
    if (!radioStats.noiseValid) {
        radioStats.noiseFloor = rssi;
        radioStats.noiseValid = true;
    } else {
        radioStats.noiseFloor = ewma(radioStats.noiseFloor, rssi, 0.1f);
    }
}

void radioStatsLine(char* buf, size_t len) {
    char noise[16];
    if (radioStats.noiseValid) snprintf(noise, sizeof(noise), "%.1f dBm", radioStats.noiseFloor);
    else strlcpy(noise, "-", sizeof(noise));
    snprintf(buf, len,
             "RX ok %lu, CRC err %lu, hdr err %lu, preamble only %lu | TX %lu (fail %lu) | "
             "LBT free %lu, busy %lu, forced %lu | noise %s",
             (unsigned long)radioStats.rxOk, (unsigned long)radioStats.crcErr,
             (unsigned long)radioStats.hdrErr, (unsigned long)radioStats.preambleOnly,
             (unsigned long)radioStats.txOk, (unsigned long)radioStats.txFail,
             (unsigned long)radioStats.lbtFree, (unsigned long)radioStats.lbtBusy,
             (unsigned long)radioStats.lbtForced, noise);
}

// ── Persisted options ───────────────────────────────────────────────────────

void loadRadioOpts() {
    RadioOpts o;
    if (prefs.getBytesLength("radioOpts") == sizeof(o) &&
        prefs.getBytes("radioOpts", &o, sizeof(o)) == sizeof(o) && o.version == 1) {
        if (o.relayDelayMode > RELAY_DELAY_RANDOM) o.relayDelayMode = RELAY_DELAY_SNR;
        if (o.freqCorrHz > 100000 || o.freqCorrHz < -100000) o.freqCorrHz = 0;
        radioOpts = o;
    } else {
        radioOpts = {1, true, RELAY_DELAY_SNR, 0};
    }
}

void saveRadioOpts() {
    radioOpts.version = 1;
    if (prefs.putBytes("radioOpts", &radioOpts, sizeof(radioOpts)) != sizeof(radioOpts)) {
        logPrintf(LOG_ERROR, "Settings", "saveRadioOpts: putBytes(\"radioOpts\") failed");
    }
}

float radioFrequencyMHz() {
    return settings.loraFrequency + (float)radioOpts.freqCorrHz / 1.0e6f;
}

bool radioPeerFreqErrorMedian(float& medianHz, uint8_t& count) {
    float v[32];
    size_t n = 0;
    for (const Peer& p : peerList) {
        if (n >= sizeof(v) / sizeof(v[0])) break;
        if (p.port == 0 && p.available && p.frqError != 0.0f) v[n++] = p.frqError;
    }
    count = (uint8_t)n;
    if (n < 3) return false;
    medianHz = medianOf(v, n);
    return true;
}

// ── Band presets / range profiles ───────────────────────────────────────────

void loraBandPreset(float freqMHz, float& bandwidthKHz, uint8_t& codingRate) {
    // Must match the "lora freq 433|868" presets (serial.cpp) and the WebUI
    if (freqMHz < 500.0f) { bandwidthKHz = 62.5f; codingRate = 6; }
    else                  { bandwidthKHz = 125.0f; codingRate = 5; }
}

int currentLoraProfile() {
    float bw; uint8_t cr;
    loraBandPreset(settings.loraFrequency, bw, cr);
    return matchLoraProfile(settings.loraSpreadingFactor, settings.loraBandwidth,
                            settings.loraCodingRate, bw, cr);
}

void printLoraProfiles() {
    float bandBw; uint8_t bandCr;
    loraBandPreset(settings.loraFrequency, bandBw, bandCr);
    size_t n;
    const LoraProfile* profiles = loraProfiles(n);
    int cur = currentLoraProfile();
    float stdSens = 0.0f;
    for (size_t i = 0; i < n; i++) {
        uint8_t sf, cr; float bw;
        loraProfileParams((int)i, bandBw, bandCr, sf, bw, cr);
        // 100 byte frame: typical text message incl. header
        uint32_t toa = computeToA(100, sf, bw, cr, settings.loraPreambleLength);
        float sens = sensitivityDbm(sf, bw);
        if (i == 0) stdSens = sens;
        char duty[48] = "";
        if (isPublicBand(settings.loraFrequency)) {
            // 10 % in 60 s = 6000 ms airtime budget per minute
            snprintf(duty, sizeof(duty), ", %.1f %% of the 6 s/min budget", toa / 60.0f);
        }
        logPrintf(LOG_INFO, "LoRa", "%c %-8s SF%u BW%.1f CR4/%u: ToA(100 B) %lu ms, sensitivity %.1f dBm (%+.1f dB)%s",
                  (int)i == cur ? '*' : ' ', profiles[i].key, sf, bw, cr, (unsigned long)toa,
                  sens, stdSens - sens, duty);
    }
    if (cur < 0) logPrintf(LOG_INFO, "LoRa", "Current settings (SF%u BW%.1f CR4/%u) match no profile (custom).",
                           settings.loraSpreadingFactor, settings.loraBandwidth, settings.loraCodingRate);
    logPrintf(LOG_INFO, "LoRa", "All nodes of a network need the same profile.");
}

bool applyLoraProfile(int index) {
    size_t n;
    loraProfiles(n);
    if (index < 0 || index >= (int)n) return false;
    float bandBw; uint8_t bandCr;
    loraBandPreset(settings.loraFrequency, bandBw, bandCr);
    loraProfileParams(index, bandBw, bandCr, settings.loraSpreadingFactor,
                      settings.loraBandwidth, settings.loraCodingRate);
    return true;
}

// ── Status output (serial `radio`) ──────────────────────────────────────────

void radioPrintStatus(bool json) {
    float snrLimit = snrLimitForSf(settings.loraSpreadingFactor);
    float fMedian = 0.0f; uint8_t fCount = 0;
    bool fValid = radioPeerFreqErrorMedian(fMedian, fCount);
    if (json) {
        JsonDocument doc;
        doc["query"] = "radio";
        JsonObject d = doc["data"].to<JsonObject>();
        d["rxOk"] = radioStats.rxOk;
        d["crcErr"] = radioStats.crcErr;
        d["hdrErr"] = radioStats.hdrErr;
        d["preambleOnly"] = radioStats.preambleOnly;
        d["txOk"] = radioStats.txOk;
        d["txFail"] = radioStats.txFail;
        d["lbtFree"] = radioStats.lbtFree;
        d["lbtBusy"] = radioStats.lbtBusy;
        d["lbtForced"] = radioStats.lbtForced;
        if (radioStats.noiseValid) d["noiseFloor"] = radioStats.noiseFloor;
        d["uptime"] = (millis() - radioStats.sinceMs) / 1000;
        d["lbt"] = radioOpts.lbt;
        d["relayDelay"] = radioOpts.relayDelayMode == RELAY_DELAY_SNR ? "snr" : "random";
        d["freqCorr"] = radioOpts.freqCorrHz;
        d["snrLimit"] = snrLimit;
        int prof = currentLoraProfile();
        if (prof >= 0) { size_t n; d["profile"] = loraProfiles(n)[prof].key; }
        else d["profile"] = "custom";
        if (fValid) d["freqErrMedian"] = fMedian;
        JsonArray arr = d["peers"].to<JsonArray>();
        for (const Peer& p : peerList) {
            if (p.port != 0) continue;
            JsonObject o = arr.add<JsonObject>();
            o["call"] = p.nodeCall;
            o["snrAvg"] = peerLinkSnr(p);
            o["margin"] = peerLinkSnr(p) - snrLimit;
            o["success"] = p.successRate;
            o["frqError"] = p.frqError;
        }
        logJson(doc);
        return;
    }
    char line[200];
    radioStatsLine(line, sizeof(line));
    logPrintf(LOG_INFO, "Radio", "%s", line);
    logPrintf(LOG_INFO, "Radio", "since %lu s | lbt %s | relaydelay %s | freqcorr %ld Hz | SF%u limit %.1f dB",
              (unsigned long)((millis() - radioStats.sinceMs) / 1000), radioOpts.lbt ? "on" : "off",
              radioOpts.relayDelayMode == RELAY_DELAY_SNR ? "snr" : "random",
              (long)radioOpts.freqCorrHz, settings.loraSpreadingFactor, snrLimit);
    for (const Peer& p : peerList) {
        if (p.port != 0) continue;
        logPrintf(LOG_INFO, "Radio", "  %-10s SNR avg %5.1f dB, reserve %5.1f dB, success %3.0f %%, freq err %6.0f Hz%s",
                  p.nodeCall, peerLinkSnr(p), peerLinkSnr(p) - snrLimit, p.successRate * 100.0f,
                  p.frqError, p.available ? "" : " (n/a)");
    }
    if (fValid) {
        bool warn = freqErrorWarn(fMedian, settings.loraBandwidth);
        logPrintf(warn ? LOG_WARN : LOG_INFO, "Radio", "Median freq error of %u neighbours: %.0f Hz (tolerance +/-%.0f Hz)%s",
                  (unsigned)fCount, fMedian, settings.loraBandwidth * 250.0f,
                  warn ? " -> calibrate with 'lora freqcorr <Hz>'" : "");
    }
}
