#ifndef MAIN_H
#define MAIN_H

#include "frame.h"
#include <vector>

// Pin-Definitionen für T3 V1.6.1
#define LORA_NSS    18
#define LORA_DIO0   26
#define LORA_RST    23
#define LORA_DIO1   33
#define PIN_WIFI_LED 25      //LED WiFi-Status (ein = AP-Mode, blinken = Client-Mode, aus = nicht verbunden)
#define AP_MODE_TASTER 0     //Taster Umschaltung WiFi CLient/AP

//Timing
#define ANNOUNCE_TIME 5 * 60 * 1000 + random(0, 1 * 60 * 1000)  //ANNOUNCE Baken
#define PEER_TIMEOUT 30 * 60 * 1000              //Zeit, nach dem ein Call aus der Peer-Liste gelöscht wird
#define ACK_TIME random(0, 4000)                 //Zeit, bis ein ACK gesendet wird
#define TX_RETRY 10                              //Retrys beim Senden 
#define TX_RETRY_TIME 4000 + random(0, 2000)     //Pause zwischen wiederholungen (muss größer als ACK_TIME sein)
#define MAX_STORED_MESSAGES 500                  //max. in "messages.json" gespeicherte Nachrichten
#define MAX_STORED_ACK 100                       //max. ACK Frames in "ack.json"

//Interner Quatsch
#define NAME "rMesh"                             //Versions-String
#define VERSION "V1.0.1-a"                       //Versions-String
#define MAX_CALLSIGN_LENGTH 8                    //maximale Länge des Rufzeichens
#define CORE_DEBUG_LEVEL 3                       // 0 bedeutet keine Logs
#define TX_FRAME_BUFFER_MAX 50

struct Peer {
    char call[17] = {0};
    time_t lastRX = 0;
    float rssi = 0;
    float snr = 0;
    float frqError = 0;
    bool available = 0;
};


extern uint32_t statusTimer;
extern const char* TZ_INFO;
extern std::vector<Peer> peerList;

extern std::vector<Frame> txFrameBuffer;
extern portMUX_TYPE txBufferMux;

#endif
