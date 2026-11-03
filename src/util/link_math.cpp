#include "util/link_math.h"
#include <cmath>
#include <cstring>
#include <algorithm>

float snrLimitForSf(uint8_t sf) {
    if (sf < 6)  sf = 6;
    if (sf > 12) sf = 12;
    return -5.0f - 2.5f * (float)(sf - 6);
}

float symbolTimeMs(uint8_t sf, float bandwidthKHz) {
    if (bandwidthKHz <= 0.0f || sf < 6 || sf > 12) return 0.0f;
    return (float)(1UL << sf) / bandwidthKHz;   // 2^SF / (BW kHz) = ms
}

float sensitivityDbm(uint8_t sf, float bandwidthKHz) {
    if (bandwidthKHz <= 0.0f) return 0.0f;
    return -174.0f + 10.0f * log10f(bandwidthKHz * 1000.0f) + 6.0f + snrLimitForSf(sf);
}

uint32_t preambleTimeoutMs(uint8_t sf, float bandwidthKHz, int16_t preambleLength) {
    float tsym = symbolTimeMs(sf, bandwidthKHz);
    if (tsym <= 0.0f) return 100;
    if (preambleLength < 6) preambleLength = 6;
    // preamble + 4.25 sync symbols + 8 header symbols + 8 symbols margin
    return (uint32_t)ceilf(((float)preambleLength + 4.25f + 8.0f + 8.0f) * tsym) + 10;
}

uint32_t lbtSlotMs(uint8_t sf, float bandwidthKHz) {
    float tsym = symbolTimeMs(sf, bandwidthKHz);
    if (tsym <= 0.0f) return 10;
    return (uint32_t)ceilf(8.5f * tsym + 1.0f);
}

uint32_t lbtBackoffMs(uint32_t slotMs, uint8_t busyStreak, uint32_t rnd) {
    if (busyStreak < 1) busyStreak = 1;
    uint8_t cw = (uint8_t)std::min<int>(3 + busyStreak - 1, 8);
    uint32_t slots = 1 + (rnd % (1UL << cw));
    return slots * slotMs;
}

uint32_t relayDelayMs(uint32_t baseMs, uint32_t windowMs, float snrMarginDb, uint32_t rnd) {
    float frac = snrMarginDb / 20.0f;
    if (!(frac > 0.0f)) frac = 0.0f;   // also catches NaN
    if (frac > 1.0f) frac = 1.0f;
    uint32_t snrPart = (uint32_t)(frac * 0.8f * (float)windowMs);
    uint32_t jitter  = windowMs / 5;
    return baseMs + snrPart + (jitter > 0 ? rnd % (jitter + 1) : 0);
}

static float retryGrowth(uint8_t attempt) {
    return std::min(powf(1.4f, (float)attempt), 4.0f);
}

uint32_t retryWindowMs(uint32_t windowMs, uint8_t attempt, uint8_t intervals) {
    if (intervals == 0) return windowMs;
    float sum = 0.0f;
    for (uint8_t k = 0; k < intervals; k++) sum += retryGrowth(k);
    float norm = (float)intervals / sum;
    return (uint32_t)((float)windowMs * retryGrowth(attempt) * norm);
}

float ewma(float prev, float sample, float alpha) {
    return prev + alpha * (sample - prev);
}

float linkSuccessUpdate(float rate, bool success) {
    return ewma(rate, success ? 1.0f : 0.0f, 0.2f);
}

float linkPenalty(float snrMarginDb, float successRate) {
    float p = 0.0f;
    if (snrMarginDb < 3.0f) {
        float m = snrMarginDb < 0.0f ? 0.0f : snrMarginDb;
        p += (3.0f - m) / 3.0f;
    }
    if (successRate < 1.0f) {
        float r = successRate < 0.0f ? 0.0f : successRate;
        p += 1.0f - r;
    }
    return p;
}

float routeMetric(uint8_t hopCount, float penalty) {
    return (float)hopCount + penalty;
}

bool routeShouldSwitch(float newMetric, float oldMetric) {
    return newMetric + ROUTE_HYSTERESIS < oldMetric;
}

float medianOf(float* v, size_t n) {
    if (n == 0) return 0.0f;
    std::sort(v, v + n);
    if (n % 2) return v[n / 2];
    return 0.5f * (v[n / 2 - 1] + v[n / 2]);
}

bool freqErrorWarn(float medianHz, float bandwidthKHz) {
    float toleranceHz = bandwidthKHz * 1000.0f / 4.0f;
    return fabsf(medianHz) > 0.2f * toleranceHz;
}

static const LoraProfile PROFILES[] = {
    {"standard", "Standard", 7,  0.0f,  0},
    {"robust",   "Robust",   9,  0.0f,  8},
    {"weit",     "Weit",     11, 62.5f, 8},
};

const LoraProfile* loraProfiles(size_t& count) {
    count = sizeof(PROFILES) / sizeof(PROFILES[0]);
    return PROFILES;
}

int findLoraProfile(const char* key) {
    if (!key) return -1;
    for (size_t i = 0; i < sizeof(PROFILES) / sizeof(PROFILES[0]); i++) {
        if (strcmp(PROFILES[i].key, key) == 0) return (int)i;
    }
    return -1;
}

void loraProfileParams(int index, float bandBwKHz, uint8_t bandCr,
                       uint8_t& sf, float& bandwidthKHz, uint8_t& codingRate) {
    if (index < 0 || index >= (int)(sizeof(PROFILES) / sizeof(PROFILES[0]))) index = 0;
    const LoraProfile& p = PROFILES[index];
    sf           = p.sf;
    bandwidthKHz = p.bandwidthKHz > 0.0f ? p.bandwidthKHz : bandBwKHz;
    codingRate   = p.codingRate   > 0    ? p.codingRate   : bandCr;
}

int matchLoraProfile(uint8_t sf, float bandwidthKHz, uint8_t codingRate,
                     float bandBwKHz, uint8_t bandCr) {
    for (int i = 0; i < (int)(sizeof(PROFILES) / sizeof(PROFILES[0])); i++) {
        uint8_t psf, pcr; float pbw;
        loraProfileParams(i, bandBwKHz, bandCr, psf, pbw, pcr);
        if (psf == sf && pcr == codingRate && fabsf(pbw - bandwidthKHz) < 0.01f) return i;
    }
    return -1;
}
