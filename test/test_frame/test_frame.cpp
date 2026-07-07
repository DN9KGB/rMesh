// Native unit tests for LoRa frame (de)serialization (src/mesh/frame.cpp).
// Exercises the attacker-controlled parse path and the export bounds/truncation
// fixes. Runs on the host via `pio test -e native` — no hardware required.
#include <unity.h>
#include "Arduino.h"          // mock

unsigned long _mock_millis = 1234;   // used by Frame's default id = millis()

// Faithful-enough stand-in for the real safeUtf8Copy: clamp to the destination
// and null-terminate. (The real one additionally avoids splitting a UTF-8
// sequence; the length-clamp + termination behaviour under test is preserved.)
void safeUtf8Copy(char* dest, const uint8_t* src, size_t srcLen, size_t dstSize) {
    if (dstSize == 0) return;
    size_t n = (srcLen < dstSize - 1) ? srcLen : (dstSize - 1);
    memcpy(dest, src, n);
    dest[n] = '\0';
}

// Compile the real implementation into this test program.
#include "mesh/frame.cpp"

void setUp(void)    {}
void tearDown(void) {}

// A MESSAGE frame survives an export -> import round trip with all fields intact.
void test_roundtrip_message(void) {
    Frame f;
    f.frameType   = Frame::MESSAGE_FRAME;
    f.messageType = Frame::TEXT_MESSAGE;
    strcpy(f.srcCall, "AB1CDE");
    strcpy(f.dstCall, "XY2Z");
    const char* txt = "hello mesh";
    memcpy(f.message, txt, strlen(txt));
    f.messageLength = strlen(txt);
    f.id       = 0x11223344u;
    f.hopCount = 2;

    uint8_t buf[255];
    size_t n = f.exportBinary(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);

    Frame g;
    g.importBinary(buf, n);
    TEST_ASSERT_EQUAL_STRING("AB1CDE", g.srcCall);
    TEST_ASSERT_EQUAL_STRING("XY2Z", g.dstCall);
    TEST_ASSERT_EQUAL_UINT8(Frame::MESSAGE_FRAME, g.frameType);
    TEST_ASSERT_EQUAL_UINT32(0x11223344u, g.id);
    TEST_ASSERT_EQUAL_UINT8(2, g.hopCount);
    TEST_ASSERT_EQUAL_size_t(strlen(txt), g.messageLength);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(txt, g.message, strlen(txt));
}

// A malicious/oversized callsign length field is clamped to MAX_CALLSIGN_LENGTH.
void test_oversized_callsign_clamped(void) {
    uint8_t buf[32];
    buf[0] = (uint8_t)(Frame::MESSAGE_FRAME & 0x0F);          // frametype, hop 0
    buf[1] = (uint8_t)((Frame::SRC_CALL_HEADER << 4) | 0x0F); // src header, len 15
    for (int i = 0; i < 15; i++) buf[2 + i] = 'A';
    Frame g;
    g.importBinary(buf, 2 + 15);
    TEST_ASSERT_LESS_OR_EQUAL(MAX_CALLSIGN_LENGTH, (int)strlen(g.srcCall));
}

// A too-short buffer is handled without crashing or writing fields.
void test_short_buffer(void) {
    uint8_t b[1] = {0x03};
    Frame g;
    g.importBinary(b, 1);        // length <= 1 -> early return
    TEST_ASSERT_EQUAL_size_t(0, strlen(g.srcCall));
}

// exportBinary never writes past the supplied buffer length.
void test_export_tiny_buffer_no_overflow(void) {
    Frame f;
    f.frameType = Frame::MESSAGE_FRAME;
    strcpy(f.srcCall, "ABCDEFGHI");
    f.messageLength = 100;
    memset(f.message, 'X', 100);
    uint8_t buf[5];
    size_t n = f.exportBinary(buf, sizeof(buf));
    TEST_ASSERT_LESS_OR_EQUAL(sizeof(buf), n);
}

// Regression: exporting into a buffer too small for the payload must NOT mutate
// the Frame's messageLength member (the truncation clamp uses a local variable),
// otherwise later retransmits from the TX buffer would carry a shortened length.
void test_export_preserves_messageLength(void) {
    Frame f;
    f.frameType = Frame::MESSAGE_FRAME;
    strcpy(f.srcCall, "ABC");
    f.messageLength = 260;
    memset(f.message, 'Y', 260);
    uint8_t buf[64];             // far too small for the full payload
    f.exportBinary(buf, sizeof(buf));
    TEST_ASSERT_EQUAL_size_t(260, f.messageLength);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_roundtrip_message);
    RUN_TEST(test_oversized_callsign_clamped);
    RUN_TEST(test_short_buffer);
    RUN_TEST(test_export_tiny_buffer_no_overflow);
    RUN_TEST(test_export_preserves_messageLength);
    return UNITY_END();
}
