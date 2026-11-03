#pragma once
#include <cstdint>
#include <cstddef>

// Pure link/channel-access math (RME-3), free of Arduino/settings/global
// dependencies so it can be unit-tested natively (see test/test_link_math).
// Randomness is passed in by the caller (rnd = any uniformly distributed value).

// ── Modem limits ─────────────────────────────────────────────────────────────

// Demodulation SNR floor of the LoRa modem for a spreading factor (Semtech
// SX126x/SX127x datasheets): SF7 -7.5 dB ... SF12 -20 dB, 2.5 dB per step.
float snrLimitForSf(uint8_t sf);

// Duration of one LoRa symbol in ms (2^SF / BW). 0 for invalid parameters.
float symbolTimeMs(uint8_t sf, float bandwidthKHz);

// Estimated receiver sensitivity in dBm: -174 + 10*log10(BW) + NF(6 dB) + SNR floor.
float sensitivityDbm(uint8_t sf, float bandwidthKHz);

// How long a detected preamble may stay latched without a valid header before
// it is treated as a false/aborted detection: preamble + header + margin.
uint32_t preambleTimeoutMs(uint8_t sf, float bandwidthKHz, int16_t preambleLength);

// ── Listen-before-talk ──────────────────────────────────────────────────────

// One backoff slot: CAD duration plus RX/TX turnaround (~8.5 symbols + 1 ms).
uint32_t lbtSlotMs(uint8_t sf, float bandwidthKHz);

// Slotted random backoff after the channel was found busy. busyStreak is the
// number of consecutive busy CADs (>= 1); the contention window doubles per
// attempt from 2^3 to 2^8 slots.
uint32_t lbtBackoffMs(uint32_t slotMs, uint8_t busyStreak, uint32_t rnd);

// After this many consecutive busy CADs the frame is sent anyway, so a
// permanently "busy" channel (noise, jammer) cannot starve the node.
static const uint8_t LBT_MAX_DEFER = 8;

// ── Relay timing ─────────────────────────────────────────────────────────────

// SNR-weighted relay delay for flooded frames: a weak reception (small margin
// above the SF floor = far away) relays early, a strong one late, so the node
// that extends the range most transmits first and nearer nodes can suppress
// their copy. baseMs is the fixed ACK window, windowMs the variable part;
// 80 % of the window is SNR-driven, 20 % stays random to break ties.
uint32_t relayDelayMs(uint32_t baseMs, uint32_t windowMs, float snrMarginDb, uint32_t rnd);

// ── Retry backoff ────────────────────────────────────────────────────────────

// Random part of the retry interval after transmission number `attempt`
// (0 = interval after the first transmission). Grows by 1.4x per attempt up
// to 4x, normalised so the sum over `intervals` attempts equals
// intervals * windowMs — the total retry time (peer timeout, cooldown) stays
// the same as with the old constant interval.
uint32_t retryWindowMs(uint32_t windowMs, uint8_t attempt, uint8_t intervals);

// ── Link quality ─────────────────────────────────────────────────────────────

// Exponentially weighted moving average.
float ewma(float prev, float sample, float alpha);

static const float LINK_SNR_ALPHA      = 0.3f;  // EWMA weight of a new SNR/RSSI sample
static const float LINK_SNR_HYSTERESIS = 2.0f;  // dB a peer must be better to overtake
static const float LINK_MINSNR_HYST    = 1.0f;  // dB above minSnr to re-enable a peer

// Success-rate EWMA (1 = every frame ACKed first try, 0 = always retried).
float linkSuccessUpdate(float rate, bool success);

// Route penalty of a next hop in "hops": up to +1 for a link with less than
// 3 dB reserve above the SF floor, up to +1 for a poor ACK success rate.
float linkPenalty(float snrMarginDb, float successRate);

// Route metric = hops + penalty of the next hop.
float routeMetric(uint8_t hopCount, float penalty);

// Switch to a new route only if it is better by more than ROUTE_HYSTERESIS.
static const float ROUTE_HYSTERESIS = 0.5f;
bool routeShouldSwitch(float newMetric, float oldMetric);

// ── Frequency error ─────────────────────────────────────────────────────────

// Median of v[0..n-1] (sorts v in place). 0 for n == 0.
float medianOf(float* v, size_t n);

// True if the median frequency error of the neighbours uses more than 20 % of
// the LoRa tolerance (±BW/4) — time to calibrate (lora freqcorr).
bool freqErrorWarn(float medianHz, float bandwidthKHz);

// ── Range profiles ───────────────────────────────────────────────────────────

struct LoraProfile {
    const char* key;      // serial/WebUI key
    const char* name;     // display name
    uint8_t sf;
    float   bandwidthKHz; // 0 = keep the band preset's bandwidth
    uint8_t codingRate;   // 0 = keep the band preset's coding rate
};

// Profile table: index 0 = "standard" (the band preset), then "robust", "weit".
const LoraProfile* loraProfiles(size_t& count);

// Index of the profile with this key, -1 if unknown.
int findLoraProfile(const char* key);

// Modem parameters of a profile for a band whose preset uses bandBw/bandCr.
void loraProfileParams(int index, float bandBwKHz, uint8_t bandCr,
                       uint8_t& sf, float& bandwidthKHz, uint8_t& codingRate);

// Index of the profile matching the given parameters, -1 = custom settings.
int matchLoraProfile(uint8_t sf, float bandwidthKHz, uint8_t codingRate,
                     float bandBwKHz, uint8_t bandCr);
