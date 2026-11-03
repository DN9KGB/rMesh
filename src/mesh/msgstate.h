#pragma once

#include <stdint.h>

// Delivery state of our own outgoing text/trace messages (#56).
//
// A small fixed table (no heap) tracks the messages we sent ourselves:
//   pending → at least one frame of the message is still in txBuffer
//   acked   → some station ACKed one of its frames (also after "failed")
//   failed  → no frame is left in txBuffer and no ACK came in
//
// The table logic is pure; the firmware plugs in two hooks: one that tells
// whether a message still has frames in txBuffer, and one that is called on
// every state change (WebSocket, msgstate.json, API, serial console).

enum MsgState : uint8_t {
    MSG_STATE_NONE    = 0,
    MSG_STATE_PENDING = 1,
    MSG_STATE_ACKED   = 2,
    MSG_STATE_FAILED  = 3,
};

#define MSG_STATE_SLOTS 16

typedef void (*MsgStateNotifyFn)(uint32_t id, uint8_t state, const char* by);
typedef bool (*MsgStateInFlightFn)(uint32_t id);

void msgStateSetHooks(MsgStateNotifyFn notify, MsgStateInFlightFn inFlight);
void msgStateReset();

/** Start (or restart, on retry) tracking a message as pending. */
void msgStateTrack(uint32_t id);

/** A station confirmed the message. Notifies only on a change. */
void msgStateAcked(uint32_t id, const char* by);

/** Turn every pending entry without frames left in txBuffer into failed. */
void msgStateSweep();

/** Current state of a message, MSG_STATE_NONE if unknown. */
uint8_t msgStateGet(uint32_t id);

const char* msgStateName(uint8_t state);
