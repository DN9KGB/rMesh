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


void sendMessage(const char* dstCall, const char* text, uint8_t messageType) {
    //Neuen Frame für alle Peers zusammenbauen
    uint8_t availableNodeCount = 0;
    Frame f;
    f.frameType = Frame::FrameTypes::MESSAGE_FRAME;
    f.messageType = messageType;
    strncpy(f.srcCall, settings.mycall, sizeof(f.srcCall));
    strncpy(f.dstCall, dstCall, sizeof(f.dstCall));
    strncpy((char*)f.message, text, sizeof(f.message));
    f.messageLength = strlen(text);
    f.id = millis();
    f.timestamp = time(NULL);
    f.tx = true;

    //An alle Peers senden
    bool first = true;
    if (txBuffer.size() > 0) {first = false;}
    for (int i = 0; i < peerList.size(); i++) {
        if (peerList[i].available) {
            availableNodeCount ++;
            memcpy(f.viaCall, peerList[i].nodeCall, sizeof(f.viaCall));
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

    //Message an Websocket senden & speichern
    char* jsonBuffer = (char*)malloc(2048);
    size_t len = f.messageJSON(jsonBuffer, 2048);
    ws.textAll(jsonBuffer, len);
    addJSONtoFile(jsonBuffer, len, "/messages.json", MAX_STORED_MESSAGES);
    free(jsonBuffer);
    jsonBuffer = nullptr;

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
    //char lineBuffer[2048]; // Puffer für eine Zeile
    char* lineBuffer = (char*)malloc(2048);
    size_t currentLine = 0;
    if (srcFile) {
        while (srcFile.available()) {
            int len = srcFile.readBytesUntil('\n', lineBuffer, 2048);
            // Nur Zeilen kopieren, die nach dem Skip-Limit liegen
            if (currentLine >= linesToSkip) {
                dstFile.write((const uint8_t*)lineBuffer, len);
                dstFile.print("\n");
            }
            currentLine++;
        }
        srcFile.close();
    }
    free(lineBuffer);
    lineBuffer = nullptr;    

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
    char* jsonBuffer = (char*)malloc(2048); 
    size_t len = serializeJson(doc,jsonBuffer, 2048);
    ws.textAll(jsonBuffer, len);  
    free(jsonBuffer);
    jsonBuffer = nullptr;
}


void availablePeerList(const char* call, bool available) {
    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return strcmp(peer.nodeCall, call) == 0; });

    if (it != peerList.end()) {
        // Peer existiert: update
        it->available = available;
    }

    //Peer Liste neu senden
    sendPeerList();
}

void addPeerList(Frame &f) {
    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return strcmp(peer.nodeCall, f.nodeCall) == 0; });

    if (it != peerList.end()) {
        // Peer existiert: update, aber available Flag behalten
        it->timestamp = f.timestamp;
        it->rssi = f.rssi;
        it->snr = f.snr;
        it->frqError = f.frqError;
        it->port = f.port;
    } else {
        // Peer nicht gefunden: hinzufügen
        Peer p;
        memcpy(p.nodeCall, f.nodeCall, sizeof(p.nodeCall));
        p.timestamp = f.timestamp;
        p.rssi = f.rssi;
        p.snr = f.snr;
        p.frqError = f.frqError;
        p.port = f.port;
        p.available = false;
        //portENTER_CRITICAL(&peerListMux);
        peerList.push_back(p);
        //portEXIT_CRITICAL(&peerListMux);

    }

    // Sortieren nach SNR (absteigend)
    std::sort(peerList.begin(), peerList.end(), [](const Peer& a, const Peer& b) { return a.snr > b.snr; });

    sendPeerList();
}

void checkPeerList() {
    // Suchen, ob Peer bereits existiert
    auto it = std::find_if(peerList.begin(), peerList.end(), [&](const Peer& peer) { return (time(NULL) - peer.timestamp) > PEER_TIMEOUT; });
    if (it != peerList.end()) {
        //portENTER_CRITICAL(&peerListMux);
        peerList.erase(it);
        //portEXIT_CRITICAL(&peerListMux);
        sendPeerList();
    } 
}

bool checkACK(const char* srcCall, const char* nodeCall, const uint32_t id) {
    //Prüfen, ob ACK Frame bereits vorhanden
    File file = LittleFS.open("/ack.json", "r");
    bool found = false;
    if (file) {
        JsonDocument doc;
        while (file.available()) {
            DeserializationError error = deserializeJson(doc, file);
            if (error == DeserializationError::Ok) {
                if ((doc["id"].as<uint32_t>() == id) && (strcmp(doc["srcCall"], srcCall) == 0) && (strcmp(doc["nodeCall"], nodeCall) == 0)) {
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
        JsonDocument doc;
        doc["srcCall"] = srcCall;
        doc["nodeCall"] = nodeCall;
        doc["id"] = id;
        char* jsonBuffer = (char*)malloc(1024);
        size_t len = serializeJson(doc, jsonBuffer, 1024);
        addJSONtoFile(jsonBuffer, len, "/ack.json", MAX_STORED_ACK);
        free(jsonBuffer);
        jsonBuffer = nullptr;
        }
    return found;
}


void safeUtf8Copy(char* dest, const uint8_t* src, size_t maxLength) {
    size_t d = 0; // Index für dest
    
    for (size_t i = 0; i < maxLength; i++) {
        uint8_t byte = src[i];

        // NEU: Abbrechen, wenn das Ende des C-Strings erreicht ist
        if (byte == 0x00) {
            break; 
        }

        // 1. Filter: 0xFF und 0xFE sind in UTF-8 niemals erlaubt
        if (byte >= 0xFE) {
            continue; // Byte überspringen
        }

        // 2. Prüfung der Byte-Sequenz
        if (byte <= 0x7F) {
            // Valides ASCII
            dest[d++] = (char)byte;
        } 
        else if (byte >= 0xC2 && byte <= 0xF4) {
            // Möglicher Start einer Mehrbyte-Sequenz
            dest[d++] = (char)byte;
        } 
        else if (byte >= 0x80 && byte <= 0xBF) {
            // Valides Folge-Byte
            dest[d++] = (char)byte;
        }
    }
    dest[d] = '\0'; // Null-Terminierung
}


void getFormattedTime(const char* format, char* outBuffer, size_t outSize) {
    time_t now = time(NULL);
    struct tm timeinfo;
    
    if (!localtime_r(&now, &timeinfo)) {
        snprintf(outBuffer, outSize, "Zeitfehler");
        return;
    }

    strftime(outBuffer, outSize, format, &timeinfo);
}


