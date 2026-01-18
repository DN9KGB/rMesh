#include "hal_LILYGO_T3_LoRa32_V1_6_1.h"
#include "RadioLib.h"
#include "settings.h"



SX1278 radio = new Module(LORA_NSS, LORA_DIO0, LORA_RST, LORA_DIO1);
#if defined(ESP8266) || defined(ESP32)
  ICACHE_RAM_ATTR
#endif
bool transmittingFlag = false;
bool receivingFlag = false;


void printState(int state) {
    if (state != RADIOLIB_ERR_NONE) {Serial.printf("FAILED! code %d\n", state);}
}


void setWiFiLED(bool value) {
    #ifdef PIN_WIFI_LED
        digitalWrite(PIN_WIFI_LED, value);
    #endif
}


void init() {
    //SPI Init
    SPI.begin(SPI_SCK, SPI_MISO, SPI_MOSI, SPI_SS);

    //Ausgäne
    pinMode(PIN_WIFI_LED, OUTPUT); 
    digitalWrite(PIN_WIFI_LED, 0); 

    //Flags zurücksetzen
    int state;

    //Init
    printState(radio.begin());
    printState(radio.setSyncWord(settings.loraSyncWord));
    printState(radio.setFrequency(settings.loraFrequency));
    printState(radio.setOutputPower(settings.loraOutputPower));
    printState(radio.setBandwidth(settings.loraBandwidth));
    printState(radio.setCodingRate(settings.loraCodingRate));
    printState(radio.setSpreadingFactor(settings.loraSpreadingFactor));
    printState(radio.setPreambleLength(settings.loraPreambleLength));
    printState(radio.setCRC(true));

    //RX einschalten
    printState(radio.startReceive());

    //Test PEER eintragen
    //Peer p;
    //p.lastRX = 0xFFFFFFFF;
    //strncpy(p.call, "DB0LUS", sizeof(p.call)-1);  //DB0LUS in p.call
    //p.available = true;
    //peerList.push_back(p);    

}


/*

bool transmitFrame(Frame &f) {
    //Senden
    transmittingFlag = true;
    statusTimer = 0;
    f.nodeCall = String(settings.mycall);
    f.tx = 1;
    f.exportBinary();
    radio.startTransmit(f.exportBinary(), f.rawDataLength);
    //Monitor
    ws.textAll(f.getMonitorJSON());
    return true;
}
*/