#include "frame.h"
#include <time.h>
#include "hal_LILYGO_T3_LoRa32_V1_6_1.h"
#include "settings.h"
#include <Arduino.h>
#include "helperFunctions.h"
#include "main.h"

Frame::Frame() {
    //Init
    timestamp = time(NULL);
    tx = false;
    messageType = MessageTypes::TEXT;
    frameType = Frame::FrameType::TEXT_MESSAGE;
    rssi = 0;
    snr = 0;
    frqError = 0;    
    hopCount = 0;
    retry = 1;
    initRetry = 1;
    messageLength = 0;
}

void Frame::importBinary(uint8_t* data, uint16_t length) {
    //lokal kopieren
    memcpy(rawData, data, length);
    rawDataLength = length;

    //Serial.printf("RX Length: %d\n", rawDataLength);
    //printHexArray(rawData, rawDataLength);
    srcCall.clear(); 
    dstCall.clear(); 
    viaCall.clear(); 
    nodeCall.clear();
    timestamp = time(NULL);
    rssi = radio.getRSSI();
    snr = radio.getSNR();
    frqError =  radio.getFrequencyError();
    tx = false;
    //Frametype
    frameType = rawData[0] & 0x0F;
    hopCount = (rawData[0] & 0xF0) >> 4;

    //Frame druchlaufen und nach Headern suchen
    uint8_t header = 0;
    uint8_t payloadLength = 0;
    for (uint8_t i=1; i < rawDataLength; i++) {
        //Header prüfen
        header = rawData[i] >> 4;
        switch (header) {
            case Frame::SRC_CALL:	
            case Frame::DST_CALL:	
            case Frame::VIA_CALL:	
            case Frame::NODE_CALL:	
                payloadLength = (rawData[i] & 0x0F);
                //Serial.printf("Call detected\n");
                //max. Länge prüfen
                if (payloadLength > MAX_CALLSIGN_LENGTH) {payloadLength = MAX_CALLSIGN_LENGTH;}
                //nicht außerhalb vom rxBuffer lesen
                if ((i + payloadLength) < rawDataLength) {
                    //Schleife von aktueller Position + 1 bis "length"
                    for(int ii = i + 1; ii <= i + payloadLength; ii++) {
                        //Einzelne Bytes in String kopieren
                        switch (header) {
                            case Frame::SRC_CALL : srcCall += (char)rawData[ii]; break;
                            case Frame::DST_CALL : dstCall += (char)rawData[ii]; break;
                            case Frame::VIA_CALL : viaCall += (char)rawData[ii]; break;
                            case Frame::NODE_CALL : nodeCall += (char)rawData[ii]; break;
                        }
                    }
                }
                //Zum nächsten Header springen (wenn Frame lange genug)
                if ((i + payloadLength) <= rawDataLength) {i += payloadLength;}
                break;
            case Frame::MESSAGE:
                //Message Type Setzen
                messageType = (rawData[i] & 0x0F);
                //ID ausschneiden
                if (rawDataLength >= (i + sizeof(id))) {
                    id = (rawData[i + 4] << 24) + (rawData[i + 3] << 16) + (rawData[i + 2] << 8) + rawData[i + 1];  
                    i += sizeof(id) + 1;
                }
                //Message Länge
                messageLength = rawDataLength - i;
                //Message
                memcpy(message, rawData + i, messageLength);
                //Nochmal als String
                messageText = byteArrayToString(message, messageLength);
                //Suche beenden
                i = rawDataLength;
        }
    }
    
}

uint8_t* Frame::exportBinary() {
   //Binärdaten zusammenbauen und senden
    rawDataLength = 0;
    //Frame-Typtext
    rawData[rawDataLength] = frameType & 0x0F;
    rawData[rawDataLength] = rawData[rawDataLength] | (hopCount & 0x0F) << 4;
    rawDataLength ++;
    //Absender
    if (srcCall.length() > 0) {
        rawData[rawDataLength] = SRC_CALL << 4 | (0x0F & srcCall.length());  //Header Absender
        rawDataLength ++;
        memcpy(&rawData[rawDataLength], &srcCall[0], srcCall.length()); //Payload
        rawDataLength += srcCall.length();
    }
    //Node hinzu
    if (nodeCall.length() > 0) {
        rawData[rawDataLength] = NODE_CALL << 4 | (0x0F & nodeCall.length());  //Header Absender
        rawDataLength ++;
        memcpy(&rawData[rawDataLength], &nodeCall[0], nodeCall.length()); //Payload
        rawDataLength += nodeCall.length();
    }
    //VIA hinzu
    if (viaCall.length() > 0) {
        rawData[rawDataLength] = VIA_CALL << 4 | (0x0F & viaCall.length());  //Header Absender
        rawDataLength ++;
        memcpy(&rawData[rawDataLength], &viaCall[0], viaCall.length()); //Payload
        rawDataLength += viaCall.length();
    }
    //Empfänger hinzu
    if (dstCall.length() > 0) {
        rawData[rawDataLength] = DST_CALL << 4 | (0x0F & dstCall.length());  //Header Absender
        rawDataLength ++;
        memcpy(&rawData[rawDataLength], &dstCall[0], dstCall.length()); //Payload
        rawDataLength += dstCall.length();
    }
    //Message hinzu (muss ganz hinten sein)
    if ((frameType == TEXT_MESSAGE) || (frameType == MESSAGE_ACK)) {
        //TYP
        rawData[rawDataLength] = HeaderType::MESSAGE << 4;      //Header TEXT_MESSAGE
        rawData[rawDataLength] = rawData[rawDataLength] | messageType;      //Header TEXT_MESSAGE
        rawDataLength ++;
        //ID
        memcpy(&rawData[rawDataLength], &id, sizeof(id)); //Payload
        rawDataLength += sizeof(id);
        //Message
        if (messageLength > settings.loraMaxMessageLength) {messageLength = settings.loraMaxMessageLength;}
        memcpy(&rawData[rawDataLength], &message[0], messageLength); //Payload
        rawDataLength += messageLength;
    }
    //Bei Frametype TUNE einfach Frame mit 0xFF auffüllen
    if (frameType == TUNE) {
        while (rawDataLength < 254) {
            rawData[rawDataLength] = 0xFF;
            rawDataLength ++;
        }
    }

    return rawData;
}


void Frame::setMessageText(String text) {
    messageLength = min(text.length(), sizeof(message) - 1);
    memcpy(message, text.c_str(), messageLength);
    message[messageLength] = '\0';
    messageText = text.substring(0, messageLength);
}


String Frame::getMonitorJSON() {
    //JSON Daten für Monitor erzeugen
    JsonDocument doc;
    for (uint16_t i = 0; i < rawDataLength; i++) {
        doc["monitor"]["rawData"][i] = rawData[i];
    }
    for (uint16_t i = 0; i < messageLength; i++) {
        doc["monitor"]["message"][i] = static_cast<uint8_t>(message[i]);
    }    
    doc["monitor"]["text"] = byteArrayToString(message, messageLength);
    doc["monitor"]["messageType"] = messageType;
    doc["monitor"]["tx"] = tx;
    doc["monitor"]["rssi"] = rssi;
    doc["monitor"]["snr"] = snr;
    doc["monitor"]["frequencyError"] = frqError;
    doc["monitor"]["time"] = timestamp;
    doc["monitor"]["srcCall"] = srcCall;
    doc["monitor"]["dstCall"] = dstCall;
    doc["monitor"]["viaCall"] = viaCall;
    doc["monitor"]["nodeCall"] = nodeCall;
    doc["monitor"]["frameType"] = frameType;
    doc["monitor"]["id"] = id;
    doc["monitor"]["hopCount"] = hopCount;
    doc["monitor"]["initRetry"] = initRetry;
    doc["monitor"]["retry"] = retry;
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    return jsonOutput;
}

String Frame::getMessageJSON() {
    if (messageType == Frame::MessageTypes::COMMAND) {return "";}
    JsonDocument doc;
    doc["message"]["text"] = byteArrayToString(message, messageLength);
    for (uint16_t i = 0; i < messageLength; i++) {
        doc["message"]["message"][i] = static_cast<uint8_t>(message[i]);
    }     
    doc["message"]["messageType"] = messageType;
    doc["message"]["srcCall"] = srcCall;
    doc["message"]["dstCall"] = dstCall;
    doc["message"]["nodeCall"] = nodeCall;
    doc["message"]["time"] = timestamp;
    doc["message"]["id"] = id;
    doc["message"]["tx"] = tx;
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    return jsonOutput;
}




