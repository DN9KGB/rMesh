#include "mesh/msgstate.h"

#include <stddef.h>

struct MsgStateEntry {
    uint32_t id;
    uint32_t seq;    // insertion order, lower = older
    uint8_t  state;
};

static MsgStateEntry      table[MSG_STATE_SLOTS];
static uint32_t           seqCounter = 0;
static MsgStateNotifyFn   notifyHook = nullptr;
static MsgStateInFlightFn inFlightHook = nullptr;

static void notify(uint32_t id, uint8_t state, const char* by) {
    if (notifyHook) notifyHook(id, state, by ? by : "");
}

static MsgStateEntry* find(uint32_t id) {
    for (size_t i = 0; i < MSG_STATE_SLOTS; i++) {
        if (table[i].state != MSG_STATE_NONE && table[i].id == id) return &table[i];
    }
    return nullptr;
}

// Pick a slot for a new entry: a free one, else the oldest finished one,
// else the oldest pending one. A pending entry pushed out is reported failed,
// so the GUI never keeps a marker that nothing will ever resolve.
static MsgStateEntry* allocate() {
    MsgStateEntry* oldestDone = nullptr;
    MsgStateEntry* oldestPending = nullptr;
    for (size_t i = 0; i < MSG_STATE_SLOTS; i++) {
        MsgStateEntry* e = &table[i];
        if (e->state == MSG_STATE_NONE) return e;
        if (e->state == MSG_STATE_PENDING) {
            if (!oldestPending || e->seq < oldestPending->seq) oldestPending = e;
        } else {
            if (!oldestDone || e->seq < oldestDone->seq) oldestDone = e;
        }
    }
    if (oldestDone) return oldestDone;
    notify(oldestPending->id, MSG_STATE_FAILED, "");
    return oldestPending;
}

void msgStateSetHooks(MsgStateNotifyFn notifyFn, MsgStateInFlightFn inFlightFn) {
    notifyHook = notifyFn;
    inFlightHook = inFlightFn;
}

void msgStateReset() {
    for (size_t i = 0; i < MSG_STATE_SLOTS; i++) table[i] = MsgStateEntry{0, 0, MSG_STATE_NONE};
    seqCounter = 0;
}

void msgStateTrack(uint32_t id) {
    MsgStateEntry* e = find(id);
    if (!e) e = allocate();
    e->id = id;
    e->seq = ++seqCounter;
    e->state = MSG_STATE_PENDING;
    notify(id, MSG_STATE_PENDING, "");
}

void msgStateAcked(uint32_t id, const char* by) {
    MsgStateEntry* e = find(id);
    if (e && e->state == MSG_STATE_ACKED) return;
    if (!e) {
        // Unknown (pushed out of the table, or sent before a reboot):
        // still worth reporting, and remembering so further ACKs stay quiet.
        e = allocate();
        e->id = id;
        e->seq = ++seqCounter;
    }
    e->state = MSG_STATE_ACKED;
    notify(id, MSG_STATE_ACKED, by);
}

void msgStateSweep() {
    if (!inFlightHook) return;
    for (size_t i = 0; i < MSG_STATE_SLOTS; i++) {
        MsgStateEntry* e = &table[i];
        if (e->state != MSG_STATE_PENDING) continue;
        if (inFlightHook(e->id)) continue;
        e->state = MSG_STATE_FAILED;
        notify(e->id, MSG_STATE_FAILED, "");
    }
}

uint8_t msgStateGet(uint32_t id) {
    MsgStateEntry* e = find(id);
    return e ? e->state : (uint8_t)MSG_STATE_NONE;
}

const char* msgStateName(uint8_t state) {
    switch (state) {
        case MSG_STATE_PENDING: return "pending";
        case MSG_STATE_ACKED:   return "acked";
        case MSG_STATE_FAILED:  return "failed";
        default:                return "none";
    }
}
