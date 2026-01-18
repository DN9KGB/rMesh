#include <Arduino.h>
 #include <LittleFS.h>
#include <esp_task_wdt.h>
// #include <time.h>
#include <vector>
// #include <HTTPClient.h>
// #include <HTTPUpdate.h>
// #include <WiFi.h>

#include "config.h"
#include "settings.h"
#include "frame.h"
#include "hal_LILYGO_T3_LoRa32_V1_6_1.h"
#include "main.h"
#include "wifiFunctions.h"
#include "webFunctions.h"
#include "serial.h"
#include "webFunctions.h"


//Uhrzeitformat
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

//Peer-Liste
std::vector<Peer> peerList;
portMUX_TYPE peerListMux = portMUX_INITIALIZER_UNLOCKED;

//Sendepuffer
std::vector<Frame> txBuffer;
portMUX_TYPE txBufferMux = portMUX_INITIALIZER_UNLOCKED;

//Timing
uint32_t announceTimer = 5000;      //Erstes Announce nach 5 Sekunden
uint32_t statusTimer = 0;
uint32_t rebootTimer = 0xFFFFFFFF;

//Anderes Zeug -> muss weg
uint16_t irqFlags = 0;



void setup() {
    //UART
    Serial.begin(115200);
    Serial.setDebugOutput(true);  
    while (!Serial) {}

    //CPU Frqg fest (soll wegen SPI sinnvoll sein)
    setCpuFrequencyMhz(240);

    //Puffer
    peerList.reserve(PEER_LIST_SIZE);
    txBuffer.reserve(TX_BUFFER_SIZE);     

    //Einstellungen laden
    loadSettings();

    //WiFI Init
    wifiInit();

    //Zeit setzzen
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    configTzTime(TZ_INFO, settings.ntpServer);
    
    //Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
    } 

    //WEB-Server starten
    startWebServer();

  //Radio Init
  //initRadio();    

    //Init OK
    Serial.printf("\n\n\n%s\nREADY.\n", PIO_ENV_NAME);   
}




void loop() {

    //UART
    checkSerialRX();



    //Reboot
    if (millis() > rebootTimer) {ESP.restart();}
}

