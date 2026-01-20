#include <Arduino.h>
#include <LittleFS.h>

#include "settings.h"
#include "helperFunctions.h"
#include "frame.h"
#include "main.h"
#include "webFunctions.h"


void printHexArray(uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      Serial.print("0"); // Führende Null für Werte kleiner als 16
    }
    Serial.print(data[i], HEX); 
    Serial.print(" "); // Leerzeichen zur besseren Lesbarkeit
  }
  Serial.println(); // Zeilenumbruch am Ende
}


void sendMessage(const char* dstCall, const char* text) {
    //Neuen Frame für alle Peers zusammenbauen
    uint8_t availableNodeCount = 0;
    Frame f;
    f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
    f.messageType = Frame::MessageTypes::TEXT_MESSAGE;
    strncpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
    strncpy(f.dstCall, dstCall, sizeof(f.dstCall));
    strncpy((char*)f.message, text, sizeof(f.message));
    f.messageLength = strlen(text);
    f.id = millis();
    f.timestamp = time(NULL);
    f.tx = true;

    // //An alle Peers senden
    // bool first = true;
    // if (txBuffer.size() > 0) {first = false;}
    // for (int i = 0; i < peerList.size(); i++) {
    //     if (peerList[i].available) {
    //         availableNodeCount ++;
    //         f.viaCall = peerList[i].call;
    //         f.retry = TX_RETRY;
    //         f.initRetry = TX_RETRY;
    //         f.syncFlag = first;
    //         txBuffer.push_back(f);
    //         first = false;
    //     }
    // } 

    //Wenn keine Peers, Frame ohne Ziel und Retry senden
    if (availableNodeCount == 0) {
        //Frame in Sendebuffer
        portENTER_CRITICAL(&txBufferMux);
        txBuffer.push_back(f);
        portEXIT_CRITICAL(&txBufferMux);
    }

    //Message an Websocket senden & speichern
    char jsonBuffer[1024];  
    size_t len = f.messageJSON(jsonBuffer, sizeof(jsonBuffer));
    ws.textAll(jsonBuffer, len);
    addJSONtoFile(jsonBuffer, len, "/messages.json", MAX_STORED_MESSAGES);
}



void addJSONtoFile(char* buffer, size_t length, const char* file, const uint16_t lines) {
    //Zeilen zählen
    size_t lineCount = 0;
    File countFile = LittleFS.open(file, "r");
    if (countFile) {
        while (countFile.available()) {
            if (countFile.read() == '\n') lineCount++;
        }
        countFile.close();
    }
    size_t linesToSkip = (lineCount >= lines) ? (lineCount - lines - 1) : 0;

    File srcFile = LittleFS.open(file, "r");   
    File dstFile = LittleFS.open("/temp.json", "w");   
    char lineBuffer[2048]; // Puffer für eine Zeile
    size_t currentLine = 0;
    if (srcFile) {
        while (srcFile.available()) {
            int len = srcFile.readBytesUntil('\n', lineBuffer, sizeof(lineBuffer));
            // Nur Zeilen kopieren, die nach dem Skip-Limit liegen
            if (currentLine >= linesToSkip) {
                dstFile.write((const uint8_t*)lineBuffer, len);
                dstFile.print("\n");
            }
            currentLine++;
        }
        srcFile.close();
    }

    if (buffer != nullptr && length > 0) {
        dstFile.write((const uint8_t*)buffer, length);
        dstFile.print("\n");
    }
    dstFile.close();

    LittleFS.remove(file);
    LittleFS.rename("/temp.json", file);
}

uint32_t getTOA(uint8_t payloadBytes) {
    //Parameter aus Settings holen
    uint8_t SF  = settings.loraSpreadingFactor;   // 7–12
    uint32_t BW = settings.loraBandwidth * 1000;  // kHz → Hz
    uint8_t CR  = settings.loraCodingRate;        // 1–4 (CR = 4/5 → 1, 4/6 → 2 ...)
    bool CRC    = true;         // true/false
    bool IH     = false;        // true/false
    uint16_t preamble = settings.loraPreambleLength;
    if (BW == 0) return 0;
    bool DE = (SF >= 11 && BW <= 125000);
    float Tsym = (float)(1 << SF) / (float)BW * 1000.0f;
    float Tpreamble = (preamble + 4.25f) * Tsym;
    float payloadBits =
        8.0f * payloadBytes
        - 4.0f * SF
        + 28.0f
        + (CRC ? 16.0f : 0.0f)
        - (IH ? 20.0f : 0.0f);
    float denominator = 4.0f * (SF - (DE ? 2 : 0));
    float payloadSymbols = 8.0f + fmaxf(ceilf(payloadBits / denominator) * (CR + 4), 0.0f);
    float total = Tpreamble + payloadSymbols * Tsym;
    return (uint32_t)roundf(total);
}


void availablePeerList(String call, bool available) {
    // //Serial.printf("availablePeerList Call:%s, available:%d\n", call, available);
    // //Available Flag in Peer-Liste setzen
    // bool availableOld = false;
    // for (int i = 0; i < peerList.size(); i++) {
    //     if (peerList[i].call == call) {
    //         availableOld =  peerList[i].available;
    //         peerList[i].available = available;
    //     }
    // }
    // //Peer Liste neu senden
    // sendPeerList();
}

void sendPeerList() {
    JsonDocument doc;
    for (int i = 0; i < peerList.size(); i++) {
        //Serial.printf("Peer List #%d %s\n", i, peerList[i].call);
        doc["peerlist"]["peers"][i]["port"] = peerList[i].port;
        doc["peerlist"]["peers"][i]["call"] = peerList[i].nodeCall;
        doc["peerlist"]["peers"][i]["timestamp"] = peerList[i].timestamp;
        doc["peerlist"]["peers"][i]["rssi"] = peerList[i].rssi;
        doc["peerlist"]["peers"][i]["snr"] = peerList[i].snr;
        doc["peerlist"]["peers"][i]["frqError"] = peerList[i].frqError;
        doc["peerlist"]["peers"][i]["available"] = peerList[i].available;
    }  
    char jsonBuffer[2048];  
    size_t len = serializeJson(doc,jsonBuffer, sizeof(jsonBuffer));
    ws.textAll(jsonBuffer, len);  
}

void addPeerList(Frame &f, bool available) {
    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return strcmp(peer.nodeCall, f.nodeCall) == 0; });

    if (it != peerList.end()) {
        // Peer existiert: update, aber available Flag behalten
        it->timestamp = f.timestamp;
        it->rssi = f.rssi;
        it->snr = f.snr;
        it->frqError = f.frqError;
        it->port = f.port;
        it->available = available; 
    } else {
        // Peer nicht gefunden: hinzufügen
        Peer p;
        memcpy(p.nodeCall, f.nodeCall, sizeof(p.nodeCall));
        p.timestamp = f.timestamp;
        p.rssi = f.rssi;
        p.snr = f.snr;
        p.frqError = f.frqError;
        p.port = f.port;
        p.available = available;       
        peerList.push_back(p);
    }

    // Sortieren nach SNR (absteigend)
    std::sort(peerList.begin(), peerList.end(), [](const Peer& a, const Peer& b) { return a.snr > b.snr; });

    sendPeerList();
}

void addPeerList(Frame &f) { //Available bleibt alt, oder false
    bool available = false;

    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return strcmp(peer.nodeCall, f.nodeCall) == 0; });
    if (it != peerList.end()) {
        // Peer existiert: update, aber available Flag behalten
        available = it->available;
    } 
    
    addPeerList(f, available);
}


void checkPeerList() {
    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (time(NULL) - peer.timestamp) > PEER_TIMEOUT; });
    if (it != peerList.end()) {
            peerList.erase(it);
            sendPeerList();
    } 
}




/*
#include "frame.h"
#include "main.h"
#include "hal.h"
#include <LittleFS.h>
#include "settings.h"







void addPeerList(Peer p) {
    // Debug-Ausgabe
    Serial.printf("addPeerList: call=%s\n", p.call);

     // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return strcmp(peer.call, p.call) == 0; });

    if (it != peerList.end()) {
        // Peer existiert: update, aber available Flag behalten
        bool availableOld = it->available;
        *it = p;                      // alle Felder updaten
        it->available = availableOld; // available nicht überschreiben
    } else {
        // Peer nicht gefunden: hinzufügen
        p.available = false;       // immer false beim Hinzufügen
        peerList.push_back(p);
    }

    // Sortieren nach SNR (absteigend)
    std::sort(peerList.begin(), peerList.end(), [](const Peer& a, const Peer& b) { return a.snr > b.snr; });

    sendPeerList();
}


void availablePeerList(String call, bool available) {
    //Serial.printf("availablePeerList Call:%s, available:%d\n", call, available);
    //Available Flag in Peer-Liste setzen
    bool availableOld = false;
    for (int i = 0; i < peerList.size(); i++) {
        if (peerList[i].call == call) {
            availableOld =  peerList[i].available;
            peerList[i].available = available;
        }
    }
    //Peer Liste neu senden
    sendPeerList();
}

void checkPeerList() {
    //Peer-Liste bereinigen
    for (int i = 0; i < peerList.size(); i++) {
        time_t age = time(NULL) - peerList[i].lastRX;
        if (age > PEER_TIMEOUT) {
            peerList.erase(peerList.begin() + i);
            sendPeerList();
            break;
        }
    }    
}


uint32_t getTOA(uint8_t payloadBytes) {
    // Parameter aus Settings holen
    uint8_t SF  = settings.loraSpreadingFactor;   // 7–12
    uint32_t BW = settings.loraBandwidth * 1000;  // kHz → Hz
    uint8_t CR  = settings.loraCodingRate;        // 1–4 (CR = 4/5 → 1, 4/6 → 2 ...)
    bool CRC    = true;         // true/false
    bool IH     = false;        // true/false
    uint16_t preamble = settings.loraPreambleLength;
    if (BW == 0) return 0;
    bool DE = (SF >= 11 && BW <= 125000);
    float Tsym = (float)(1 << SF) / (float)BW * 1000.0f;
    float Tpreamble = (preamble + 4.25f) * Tsym;
    float payloadBits =
        8.0f * payloadBytes
        - 4.0f * SF
        + 28.0f
        + (CRC ? 16.0f : 0.0f)
        - (IH ? 20.0f : 0.0f);
    float denominator = 4.0f * (SF - (DE ? 2 : 0));
    float payloadSymbols = 8.0f + fmaxf(ceilf(payloadBits / denominator) * (CR + 4), 0.0f);
    float total = Tpreamble + payloadSymbols * Tsym;
    return (uint32_t)roundf(total);
}

String ackJson(String srcCall, String nodeCall, uint32_t id) {
    JsonDocument doc;
    doc["srcCall"] = srcCall;
    doc["nodeCall"] = nodeCall;
    doc["id"] = id;
    String jsonOutput;
    serializeJson(doc, jsonOutput);
    return jsonOutput;
}



bool checkACK(String srcCall, String nodeCall, uint32_t id) {
    //Prüfen, ob ACK Frame bereits vorhanden
    File file = LittleFS.open("/ack.json", "r");
    bool found = false;
    if (file) {
        JsonDocument doc;
        while (file.available()) {
            DeserializationError error = deserializeJson(doc, file);
            if (error == DeserializationError::Ok) {
                if ((doc["id"].as<uint32_t>() == id) && (doc["srcCall"].as<String>() == srcCall) && (doc["nodeCall"].as<String>() == nodeCall)) {
                    found = true;
                    break; 
                }
            } else if (error != DeserializationError::EmptyInput) {
                file.readStringUntil('\n');
            }
        }
        file.close();                    
    }

    //Message ACK in Datei speichern
    if (found == false) {
        String json = ackJson(srcCall, nodeCall, id);
        file = LittleFS.open("/ack.json", "a"); 
        if (file) {
            file.println(json); 
            file.close();
            limitFileLines("/ack.json", MAX_STORED_ACK);
        }
    }
    return found;
}

void sendTrace(String dstCall, String text) {
    //Neuen Frame für alle Peers zusammenbauen
    uint8_t availableNodeCount = 0;
    Frame f;
    f.frameType = Frame::TEXT_MESSAGE;
    f.messageType = Frame::MessageTypes::TRACE;
    f.srcCall = String(settings.mycall);
    f.dstCall = dstCall;
    f.setMessageText(text);
    f.id = millis();
    f.timestamp = time(NULL);
    f.tx = true;

    //An alle Peers senden
    bool first = true;
    if (txBuffer.size() > 0) {first = false;}
    for (int i = 0; i < peerList.size(); i++) {
        if (peerList[i].available) {
            availableNodeCount ++;
            f.viaCall = peerList[i].call;
            f.retry = TX_RETRY;
            f.initRetry = TX_RETRY;
            f.syncFlag = first;
            txBuffer.push_back(f);
            first = false;
        }
    } 

    //Wenn keine Peers, Frame ohne Ziel und Retry senden
    if (availableNodeCount == 0) {
        //Frame in Sendebuffer
        txBuffer.push_back(f);
    }

    //Über Websocket (zurück) senden
    String json;
    json = f.getMessageJSON();
    ws.textAll(json);    

    //Message in Datei speichern
    File file = LittleFS.open("/messages.json", "a"); 
    if (file) {
        file.println(json); 
        file.close();
        limitFileLines("/messages.json", MAX_STORED_MESSAGES);
    }    

}

void sendMessage(String dstCall, String text) {
    //Neuen Frame für alle Peers zusammenbauen
    uint8_t availableNodeCount = 0;
    Frame f;
    f.frameType = Frame::TEXT_MESSAGE;
    f.messageType = Frame::MessageTypes::TEXT;
    f.srcCall = String(settings.mycall);
    f.dstCall = dstCall;
    f.setMessageText(text);
    f.id = millis();
    f.timestamp = time(NULL);
    f.tx = true;

    //An alle Peers senden
    bool first = true;
    if (txBuffer.size() > 0) {first = false;}
    for (int i = 0; i < peerList.size(); i++) {
        if (peerList[i].available) {
            availableNodeCount ++;
            f.viaCall = peerList[i].call;
            f.retry = TX_RETRY;
            f.initRetry = TX_RETRY;
            f.syncFlag = first;
            txBuffer.push_back(f);
            first = false;
        }
    } 

    //Wenn keine Peers, Frame ohne Ziel und Retry senden
    if (availableNodeCount == 0) {
        //Frame in Sendebuffer

        portENTER_CRITICAL(&txBufferMux);
            if (txBuffer.size() >= TX_BUFFER_SIZE) {
        // ältesten Frame löschen (Index 0)
        txBuffer.erase(txBuffer.begin());
    }
    txBuffer.push_back(f);

        portEXIT_CRITICAL(&txBufferMux);

        txBuffer.push_back(f);
    }

    //Über Websocket (zurück) senden
    String json;
    json = f.getMessageJSON();
    ws.textAll(json);    

    //Message in Datei speichern
    File file = LittleFS.open("/messages.json", "a"); 
    if (file) {
        file.println(json); 
        file.close();
        limitFileLines("/messages.json", MAX_STORED_MESSAGES);
    }    
}




void sendFrame(Frame &f) {
    f.tx = true;
    f.id = millis();
    f.timestamp = time(NULL);
    f.nodeCall = String(settings.mycall);
    txBuffer.push_back(f);

    //Über Websocket (zurück) senden
    String json;
    json = f.getMessageJSON();
    ws.textAll(json);    

    //Message in Datei speichern
    File file = LittleFS.open("/messages.json", "a"); 
    if (file) {
        file.println(json); 
        file.close();
        limitFileLines("/messages.json", MAX_STORED_MESSAGES);
    }    

}




String getFormattedTime(const char* format) {
    time_t now = time(NULL);
    struct tm timeinfo;
    // Lokale Zeitstruktur füllen
    if (!localtime_r(&now, &timeinfo)) {
        return "Zeitfehler";
    }
    // Puffer für das Ergebnis (64 Zeichen reichen für fast alle Formate)
    char buffer[64];
    strftime(buffer, sizeof(buffer), format, &timeinfo);
    return String(buffer);
}



void printHexArray(uint8_t* data, size_t length) {
  for (size_t i = 0; i < length; i++) {
    if (data[i] < 0x10) {
      Serial.print("0"); // Führende Null für Werte kleiner als 16
    }
    Serial.print(data[i], HEX); 
    Serial.print(" "); // Leerzeichen zur besseren Lesbarkeit
  }
  Serial.println(); // Zeilenumbruch am Ende
}

void limitFileLines(const char* path, int maxLines) {
    File file = LittleFS.open(path, "r");
    if (!file) return;
   int count = 0;
    while (file.available()) {
        if (file.read() == '\n') count++;
    }
    if (count <= maxLines) {
        file.close();
        return;
    }
    int skipLines = count - (maxLines - 50); // Wir behalten 950, um Puffer zu haben
    file.seek(0);
    File tempFile = LittleFS.open("/temp.json", "w");
    int currentLine = 0;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (currentLine >= skipLines) {
            tempFile.println(line);
        }
        currentLine++;
    }
    file.close();
    tempFile.close();
    LittleFS.remove(path);
    LittleFS.rename("/temp.json", path);
}

*/