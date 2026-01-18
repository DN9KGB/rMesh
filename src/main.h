#pragma once

struct Peer {
    char call[MAX_CALLSIGN_LENGTH + 1] = {0};
    time_t lastRX = 0;
    float rssi = 0;
    float snr = 0;
    float frqError = 0;
    bool available = 0;
};

extern uint32_t rebootTimer;
extern uint32_t statusTimer;
extern uint32_t announceTimer;
extern const char* TZ_INFO;
extern std::vector<Peer> peerList;
extern portMUX_TYPE peerListMux;
//extern std::vector<Frame> txBuffer;
extern portMUX_TYPE txBufferMux;

