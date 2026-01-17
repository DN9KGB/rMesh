#include "helperFunctions.h"
#include "frame.h"
#include "main.h"
#include "hal_LILYGO_T3_LoRa32_V1_6_1.h"
#include <LittleFS.h>
#include "settings.h"





void sendPeerList() {
  JsonDocument doc;
  for (int i = 0; i < peerList.size(); i++) {
    //Serial.printf("Peer List #%d %s\n", i, peerList[i].call);
    doc["peerlist"]["peers"][i]["call"] = peerList[i].call;
    doc["peerlist"]["peers"][i]["lastRX"] = peerList[i].lastRX;
    doc["peerlist"]["peers"][i]["rssi"] = peerList[i].rssi;
    doc["peerlist"]["peers"][i]["snr"] = peerList[i].snr;
    doc["peerlist"]["peers"][i]["frqError"] = peerList[i].frqError;
    doc["peerlist"]["peers"][i]["available"] = peerList[i].available;
  }  
  doc["peerlist"]["count"] = peerList.size();
  String jsonOutput;
  serializeJson(doc, jsonOutput);
  ws.textAll(jsonOutput);
}

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
    if (txFrameBuffer.size() > 0) {first = false;}
    for (int i = 0; i < peerList.size(); i++) {
        if (peerList[i].available) {
            availableNodeCount ++;
            f.viaCall = peerList[i].call;
            f.retry = TX_RETRY;
            f.initRetry = TX_RETRY;
            f.syncFlag = first;
            txFrameBuffer.push_back(f);
            first = false;
        }
    } 

    //Wenn keine Peers, Frame ohne Ziel und Retry senden
    if (availableNodeCount == 0) {
        //Frame in Sendebuffer
        txFrameBuffer.push_back(f);
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
    if (txFrameBuffer.size() > 0) {first = false;}
    for (int i = 0; i < peerList.size(); i++) {
        if (peerList[i].available) {
            availableNodeCount ++;
            f.viaCall = peerList[i].call;
            f.retry = TX_RETRY;
            f.initRetry = TX_RETRY;
            f.syncFlag = first;
            txFrameBuffer.push_back(f);
            first = false;
        }
    } 

    //Wenn keine Peers, Frame ohne Ziel und Retry senden
    if (availableNodeCount == 0) {
        //Frame in Sendebuffer

        portENTER_CRITICAL(&txBufferMux);
            if (txFrameBuffer.size() >= TX_FRAME_BUFFER_MAX) {
        // ältesten Frame löschen (Index 0)
        txFrameBuffer.erase(txFrameBuffer.begin());
    }
    txFrameBuffer.push_back(f);

        portEXIT_CRITICAL(&txBufferMux);

        txFrameBuffer.push_back(f);
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


String byteArrayToString(uint8_t* buffer, size_t length) {
    if (buffer == nullptr || length == 0) return "";    
    String s = "";
    for (int i = 0; i < length; i++) {
        if ((buffer[i] >= 0xF5) && (buffer[i] <= 0xFF)) {break;}
        if ((buffer[i] >= 0xC0) && (buffer[i] <= 0xC1)) {break;}
        s += (char)buffer[i];
    }
    return s;
}

void sendFrame(Frame &f) {
    f.tx = true;
    f.id = millis();
    f.timestamp = time(NULL);
    f.nodeCall = String(settings.mycall);
    txFrameBuffer.push_back(f);

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

