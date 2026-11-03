// Native unit tests for the link / channel-access math (src/util/link_math.cpp).
#include <unity.h>
#include "util/link_math.cpp"

void setUp(void)    {}
void tearDown(void) {}

void test_snr_limit_per_sf(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -7.5f,  snrLimitForSf(7));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -12.5f, snrLimitForSf(9));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -20.0f, snrLimitForSf(12));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -20.0f, snrLimitForSf(99));  // clamped
}

void test_symbol_time_and_sensitivity(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.048f, symbolTimeMs(7, 62.5f));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f,   symbolTimeMs(7, 0.0f));
    // SF7/BW125: -174 + 51 + 6 - 7.5 = -124.5 dBm
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -124.5f, sensitivityDbm(7, 125.0f));
    // each SF step +2.5 dB, BW halving +3 dB
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 2.5f, sensitivityDbm(7, 125.0f) - sensitivityDbm(8, 125.0f));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 3.0f, sensitivityDbm(7, 125.0f) - sensitivityDbm(7, 62.5f));
}

void test_preamble_timeout_covers_header(void) {
    // SF7/BW62.5, preamble 10: (10+4.25+16) symbols * 2.048 ms = 62 ms (+10)
    uint32_t t = preambleTimeoutMs(7, 62.5f, 10);
    TEST_ASSERT_UINT32_WITHIN(2, 72, t);
    TEST_ASSERT_GREATER_THAN(t, preambleTimeoutMs(12, 62.5f, 10));
}

void test_lbt_backoff_window_grows(void) {
    uint32_t slot = lbtSlotMs(7, 62.5f);           // 8.5 * 2.048 + 1 -> 19 ms
    TEST_ASSERT_EQUAL_UINT32(19, slot);
    // first busy: 1..8 slots
    TEST_ASSERT_EQUAL_UINT32(1 * slot, lbtBackoffMs(slot, 1, 0));
    TEST_ASSERT_EQUAL_UINT32(8 * slot, lbtBackoffMs(slot, 1, 7));
    TEST_ASSERT_EQUAL_UINT32(1 * slot, lbtBackoffMs(slot, 1, 8));   // wraps
    // window doubles per busy CAD, capped at 2^8 slots
    TEST_ASSERT_EQUAL_UINT32(16 * slot,  lbtBackoffMs(slot, 2, 15));
    TEST_ASSERT_EQUAL_UINT32(256 * slot, lbtBackoffMs(slot, 6, 255));
    TEST_ASSERT_EQUAL_UINT32(256 * slot, lbtBackoffMs(slot, 200, 0xFFFFFFFF));
    TEST_ASSERT_EQUAL_UINT32(1 * slot,   lbtBackoffMs(slot, 0, 0));  // streak 0 treated as 1
}

void test_relay_delay_weak_snr_first(void) {
    uint32_t base = 1000, win = 2000;
    uint32_t weak   = relayDelayMs(base, win, 0.0f,  0);
    uint32_t mid    = relayDelayMs(base, win, 10.0f, 0);
    uint32_t strong = relayDelayMs(base, win, 30.0f, 0);
    TEST_ASSERT_EQUAL_UINT32(1000, weak);
    TEST_ASSERT_EQUAL_UINT32(1800, mid);            // 0.5 * 0.8 * 2000
    TEST_ASSERT_EQUAL_UINT32(2600, strong);         // capped at 80 %
    TEST_ASSERT_EQUAL_UINT32(1000, relayDelayMs(base, win, -5.0f, 0));   // below floor
    TEST_ASSERT_EQUAL_UINT32(1000, relayDelayMs(base, win, NAN, 0));
    // jitter stays within 20 % of the window
    TEST_ASSERT_EQUAL_UINT32(1400, relayDelayMs(base, win, 0.0f, 400));
    TEST_ASSERT_EQUAL_UINT32(1000, relayDelayMs(base, win, 0.0f, 401));
    // never later than base + window
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(base + win, relayDelayMs(base, win, 99.0f, 400));
}

void test_retry_window_grows_and_keeps_total(void) {
    const uint32_t win = 1000;
    const uint8_t n = 9;
    uint32_t sum = 0, prev = 0;
    for (uint8_t k = 0; k < n; k++) {
        uint32_t w = retryWindowMs(win, k, n);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT32(prev, w);   // non-decreasing
        prev = w;
        sum += w;
    }
    TEST_ASSERT_UINT32_WITHIN(n, n * win, sum);         // total unchanged
    TEST_ASSERT_LESS_THAN_UINT32(win, retryWindowMs(win, 0, n));      // first retry sooner
    TEST_ASSERT_GREATER_THAN_UINT32(win, retryWindowMs(win, n - 1, n)); // last later
    TEST_ASSERT_EQUAL_UINT32(win, retryWindowMs(win, 3, 0));          // degenerate
}

void test_ewma_and_success_rate(void) {
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 3.0f, ewma(0.0f, 10.0f, 0.3f));
    float r = 1.0f;
    r = linkSuccessUpdate(r, false);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.8f, r);
    r = linkSuccessUpdate(r, true);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.84f, r);
}

void test_route_metric_prefers_good_links(void) {
    // 1 hop over a link at the SF floor with poor success vs. 2 clean hops
    float bad  = routeMetric(1, linkPenalty(0.0f, 0.5f));    // 1 + 1 + 0.5
    float good = routeMetric(2, linkPenalty(10.0f, 1.0f));   // 2
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, bad);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, good);
    TEST_ASSERT_TRUE(routeShouldSwitch(good - 0.01f, bad));
    // hysteresis: only clearly better routes win
    TEST_ASSERT_FALSE(routeShouldSwitch(2.0f, 2.4f));
    TEST_ASSERT_TRUE(routeShouldSwitch(1.0f, 2.0f));
    // healthy link: no penalty at all
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, linkPenalty(3.0f, 1.0f));
}

void test_median_and_freq_warning(void) {
    float a[] = {300.0f, -100.0f, 200.0f};
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 200.0f, medianOf(a, 3));
    float b[] = {4.0f, 1.0f, 3.0f, 2.0f};
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, medianOf(b, 4));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, medianOf(nullptr, 0));
    // BW 62.5: tolerance 15625 Hz, 20 % = 3125 Hz
    TEST_ASSERT_FALSE(freqErrorWarn(3000.0f, 62.5f));
    TEST_ASSERT_TRUE(freqErrorWarn(-3200.0f, 62.5f));
}

void test_profiles(void) {
    size_t n = 0;
    loraProfiles(n);
    TEST_ASSERT_EQUAL(3, (int)n);
    TEST_ASSERT_EQUAL(1, findLoraProfile("robust"));
    TEST_ASSERT_EQUAL(-1, findLoraProfile("turbo"));
    uint8_t sf, cr; float bw;
    loraProfileParams(0, 125.0f, 5, sf, bw, cr);          // standard keeps band preset
    TEST_ASSERT_EQUAL_UINT8(7, sf); TEST_ASSERT_EQUAL_UINT8(5, cr);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 125.0f, bw);
    loraProfileParams(1, 62.5f, 6, sf, bw, cr);           // robust
    TEST_ASSERT_EQUAL_UINT8(9, sf); TEST_ASSERT_EQUAL_UINT8(8, cr);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 62.5f, bw);
    loraProfileParams(2, 125.0f, 5, sf, bw, cr);          // weit
    TEST_ASSERT_EQUAL_UINT8(11, sf); TEST_ASSERT_EQUAL_UINT8(8, cr);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 62.5f, bw);
    TEST_ASSERT_EQUAL(2,  matchLoraProfile(11, 62.5f, 8, 125.0f, 5));
    TEST_ASSERT_EQUAL(0,  matchLoraProfile(7, 62.5f, 6, 62.5f, 6));
    TEST_ASSERT_EQUAL(-1, matchLoraProfile(8, 62.5f, 6, 62.5f, 6));   // custom
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_snr_limit_per_sf);
    RUN_TEST(test_symbol_time_and_sensitivity);
    RUN_TEST(test_preamble_timeout_covers_header);
    RUN_TEST(test_lbt_backoff_window_grows);
    RUN_TEST(test_relay_delay_weak_snr_first);
    RUN_TEST(test_retry_window_grows_and_keeps_total);
    RUN_TEST(test_ewma_and_success_rate);
    RUN_TEST(test_route_metric_prefers_good_links);
    RUN_TEST(test_median_and_freq_warning);
    RUN_TEST(test_profiles);
    return UNITY_END();
}
