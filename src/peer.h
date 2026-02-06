#pragma once

#include "frame.h"

struct Peer {
    char nodeCall[MAX_CALLSIGN_LENGTH + 1] = {0};
    time_t timestamp = 0;
    float rssi = 0;
    float snr = 0;
    float frqError = 0;
    bool available = 0;
    uint8_t port = 0;
};

void checkPeerList();
void availablePeerList(const char* call, bool available, uint8_t port);
void addPeerList(Frame &f);
void sendPeerList();

extern std::vector<Peer> peerList;

