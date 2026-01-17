#ifndef BLINKER_H
#define BLINKER_H

#include <Arduino.h>



class Frame {
  private:

    // char srcCall[17];
    // char dstCall[17];
    // char nodeCall[17];
    // char viaCall[17];
    // char messageText[128];

  public:
    uint16_t _rawDataLength;
    String messageText;
    uint8_t retry;
    uint8_t initRetry;
    uint8_t hopCount;

    String srcCall;
    String dstCall;
    String nodeCall;
    String viaCall;  

    uint32_t transmitMillis = 0;
    uint8_t frameType = 0x00;
    uint8_t messageType = 1;
    uint8_t message[256];
    uint16_t messageLength = 0;
    uint32_t id = 0;
    uint8_t rawData[256];
    uint16_t rawDataLength = 0;
    time_t timestamp = 0;
    float rssi = 0;
    float snr = 0;
    float frqError = 0;
    bool tx = false;
    bool syncFlag = false;

    enum FrameType {
        ANNOUNCE,  
        ANNOUNCE_ACK,
        TUNE,
        TEXT_MESSAGE,
        MESSAGE_ACK
    };

    enum MessageTypes {
        //Untere 4 Bits vom Header-Byte -> 0x00 bis 0x0F; Nur Bei MESSAGE-HEADER (sonst ist das Payload-Length)
        TEXT = 0,
        TRACE = 1,
        COMMAND = 15   //Fernsteuerbefehle für Node:  0xFF:Version, 0xFE:Reboot
    };

    enum HeaderType {
        //Obere 4 Bits vom Header-Byte -> 0x00 bis 0x0F
        SRC_CALL,  
        DST_CALL,
        MESSAGE,
        NODE_CALL,
        VIA_CALL
    };

    // Konstruktor: Wird beim Erstellen aufgerufen
    Frame();

    void importBinary(uint8_t* data, uint16_t length);
    uint8_t* exportBinary();
    String getMonitorJSON();
    String getMessageJSON();
    void setMessageText(String text);
        
};



#endif
