// Native unit tests for the own-message delivery state table (src/mesh/msgstate.cpp).
#include <unity.h>
#include <string.h>
#include "mesh/msgstate.cpp"

// Fake txBuffer: the set of ids that still have frames queued.
static uint32_t inFlight[32];
static int inFlightCount = 0;

static bool fakeInFlight(uint32_t id) {
    for (int i = 0; i < inFlightCount; i++) if (inFlight[i] == id) return true;
    return false;
}
static void queue(uint32_t id) { inFlight[inFlightCount++] = id; }
static void drop(uint32_t id) {
    for (int i = 0; i < inFlightCount; i++) {
        if (inFlight[i] == id) { inFlight[i] = inFlight[--inFlightCount]; return; }
    }
}

// Recorded notifications
struct Note { uint32_t id; uint8_t state; char by[16]; };
static Note notes[64];
static int noteCount = 0;

static void fakeNotify(uint32_t id, uint8_t state, const char* by) {
    Note& n = notes[noteCount++];
    n.id = id; n.state = state;
    strncpy(n.by, by, sizeof(n.by) - 1); n.by[sizeof(n.by) - 1] = 0;
}

void setUp(void) {
    msgStateReset();
    msgStateSetHooks(fakeNotify, fakeInFlight);
    inFlightCount = 0;
    noteCount = 0;
}
void tearDown(void) {}

void test_track_is_pending(void) {
    queue(1);
    msgStateTrack(1);
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_PENDING, msgStateGet(1));
    TEST_ASSERT_EQUAL_INT(1, noteCount);
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_PENDING, notes[0].state);
}

void test_sweep_keeps_inflight_pending(void) {
    queue(1);
    msgStateTrack(1);
    msgStateSweep();
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_PENDING, msgStateGet(1));
    TEST_ASSERT_EQUAL_INT(1, noteCount);
}

void test_ack_marks_acked_once(void) {
    queue(1);
    msgStateTrack(1);
    drop(1);
    msgStateAcked(1, "DB0ABC");
    msgStateAcked(1, "DB0XYZ");
    msgStateSweep();
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_ACKED, msgStateGet(1));
    TEST_ASSERT_EQUAL_INT(2, noteCount);
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_ACKED, notes[1].state);
    TEST_ASSERT_EQUAL_STRING("DB0ABC", notes[1].by);
}

void test_sweep_without_frames_fails(void) {
    queue(1);
    msgStateTrack(1);
    drop(1);
    msgStateSweep();
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_FAILED, msgStateGet(1));
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_FAILED, notes[1].state);
    // A second sweep does not report again
    msgStateSweep();
    TEST_ASSERT_EQUAL_INT(2, noteCount);
}

void test_nothing_queued_fails(void) {
    msgStateTrack(7);   // e.g. txBuffer was full
    msgStateSweep();
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_FAILED, msgStateGet(7));
}

void test_late_ack_after_failed(void) {
    msgStateTrack(1);
    msgStateSweep();
    msgStateAcked(1, "DB0ABC");
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_ACKED, msgStateGet(1));
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_ACKED, notes[noteCount - 1].state);
}

void test_retry_goes_back_to_pending(void) {
    msgStateTrack(1);
    msgStateSweep();
    queue(1);
    msgStateTrack(1);
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_PENDING, msgStateGet(1));
    drop(1);
    msgStateSweep();
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_FAILED, msgStateGet(1));
}

void test_unknown_ack_is_reported(void) {
    msgStateAcked(99, "DB0ABC");
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_ACKED, msgStateGet(99));
    TEST_ASSERT_EQUAL_INT(1, noteCount);
}

void test_overflow_drops_finished_first(void) {
    // Slot 1 finished (acked), the rest pending
    msgStateTrack(1);
    msgStateAcked(1, "X");
    for (uint32_t id = 2; id <= MSG_STATE_SLOTS; id++) { queue(id); msgStateTrack(id); }
    noteCount = 0;
    queue(100);
    msgStateTrack(100);
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_NONE, msgStateGet(1));
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_PENDING, msgStateGet(2));
    TEST_ASSERT_EQUAL_INT(1, noteCount);   // only the new pending
}

void test_overflow_all_pending_reports_oldest_failed(void) {
    for (uint32_t id = 1; id <= MSG_STATE_SLOTS; id++) { queue(id); msgStateTrack(id); }
    noteCount = 0;
    queue(100);
    msgStateTrack(100);
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_NONE, msgStateGet(1));
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_PENDING, msgStateGet(100));
    TEST_ASSERT_EQUAL_INT(2, noteCount);
    TEST_ASSERT_EQUAL_UINT32(1, notes[0].id);
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_FAILED, notes[0].state);
}

void test_retrack_does_not_duplicate_entry(void) {
    queue(5);
    for (int i = 0; i < 40; i++) msgStateTrack(5);
    // Table still has room for 15 more distinct ids without eviction
    for (uint32_t id = 10; id < 10 + MSG_STATE_SLOTS - 1; id++) { queue(id); msgStateTrack(id); }
    TEST_ASSERT_EQUAL_UINT8(MSG_STATE_PENDING, msgStateGet(5));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_track_is_pending);
    RUN_TEST(test_sweep_keeps_inflight_pending);
    RUN_TEST(test_ack_marks_acked_once);
    RUN_TEST(test_sweep_without_frames_fails);
    RUN_TEST(test_nothing_queued_fails);
    RUN_TEST(test_late_ack_after_failed);
    RUN_TEST(test_retry_goes_back_to_pending);
    RUN_TEST(test_unknown_ack_is_reported);
    RUN_TEST(test_overflow_drops_finished_first);
    RUN_TEST(test_overflow_all_pending_reports_oldest_failed);
    RUN_TEST(test_retrack_does_not_duplicate_entry);
    return UNITY_END();
}
