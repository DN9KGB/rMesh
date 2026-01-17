#include <Arduino.h>
#include <main.h>
#include <AsyncTCP.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "wifiFunctions.h"
#include "webFunctions.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#include "settings.h"
#include "time.h"
#include "hal_LILYGO_T3_LoRa32_V1_6_1.h"
#include <RadioLib.h>
#include "serial.h"
#include "frame.h"
#include "helperFunctions.h"


//Routing
uint32_t announceTimer = 5000; //Erstes Announce nach 5 Sekunden

//Uhrzeit 
const char* TZ_INFO = "CET-1CEST,M3.5.0,M10.5.0/3";

//Sendepuffer
std::vector<Peer> peerList;

std::vector<Frame> txFrameBuffer;
portMUX_TYPE txBufferMux = portMUX_INITIALIZER_UNLOCKED;

//Timing
uint32_t statusTimer = millis();
uint32_t rebootTimer = 0xFFFFFFFF;
uint16_t irqFlags = 0;

void setup() {

    #ifdef HELTEC_WIRELESS_STICK_LITE_V3
    delay(1000);
    Serial.begin(115200);
    Serial.setDebugOutput(true);  
    while (!Serial) {}
    Serial.printf("\n\n\n");
    Serial.printf("HALLO");
    Serial.printf("HALLO");
    while (true) {}
    #endif

  //Buffer
  peerList.reserve(10);
  txFrameBuffer.reserve(TX_FRAME_BUFFER_MAX);    

  //CPU Frqg fest (soll wegen SPI sinnvoll sein)
  setCpuFrequencyMhz(240);

  //SPI Init
  SPI.begin(5, 19, 27, 18);

  //Ausgäne
  pinMode(PIN_WIFI_LED, OUTPUT); 
  digitalWrite(PIN_WIFI_LED, 0); 

  //Debug-Port
  Serial.begin(115200);
  Serial.setDebugOutput(true);  
  while (!Serial) {}
  Serial.printf("\n\n\n");
  
  //Einstellungen laden
  loadSettings();
  if (checkSettings() == false) { defaultSettings(); }

  //WiFi Starten
  wifiInit();

  //Zeit setzzen
  struct timeval tv = { .tv_sec = 0, .tv_usec = 0 };
  settimeofday(&tv, NULL);
  configTzTime(TZ_INFO, settings.ntpServer);

  // Initialize LittleFS
  if (!LittleFS.begin(true)) {
	Serial.println("An error has occurred while mounting LittleFS");
  } 

  //WEB-Server starten
  startWebServer();

  //Radio Init
  initRadio();

  Serial.println ("     **** COMMODORE 64 BASIC V2 ****");
  Serial.println ("  64K RAM SYSTEM  38911 BASIC BYTES FREE");
  Serial.println ("READY.");
}




void loop() {
    //Uart abfragen
    checkSerialRX();

	//WiFi Status anzeigen (LED)
	showWiFiStatus();  

	//Announce Senden
	if (millis() > announceTimer) {
		announceTimer = millis() + ANNOUNCE_TIME;
		Frame f;
		f.frameType = Frame::ANNOUNCE;
		f.transmitMillis = 0;
		//Frame in SendeBuffer
		txFrameBuffer.push_back(f);
	}

    //IRQ-Flags auslesen
    irqFlags = radio.getIRQFlags();

    //Prüfen ob Kanal belegt
    if (irqFlags & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_VALID_HEADER) {
        if (receivingFlag == false) {
            receivingFlag = true;
            statusTimer = 0;
        }
    } else {
        if (receivingFlag == true) {
            receivingFlag = false;
            statusTimer = 0;
        }
    }

	//Prüfen, ob was gesendet werden muss
	if ((transmittingFlag == false) && (receivingFlag == false)) {
		//Frames mit retry > 1 werden synchron gesendet !!!
        //Prüfen, ob es Frames gibt, die noch nicht synchron gesendet wurden
        bool sendNewSyncFrame = true;
        for (int i = 0; i < txFrameBuffer.size(); i++) {
            if (txFrameBuffer[i].syncFlag == true) {sendNewSyncFrame = false;}
        }

        //Im Puffer nach synchronen Frames duchen und den 1. gefundenen zum Senden freigeben
        if (sendNewSyncFrame == true) {
            for (int i = 0; i < txFrameBuffer.size(); i++) {
                if(txFrameBuffer[i].retry > 1) {
                    txFrameBuffer[i].syncFlag = true; 
                    txFrameBuffer[i].transmitMillis = millis() + TX_RETRY_TIME + getTOA(30); //Time On Air für Antwort
                    break;   
                }
            }
        }
        
        //Sendepuffer duchlaufen und ggg. Frames senden
		for (int i = 0; i < txFrameBuffer.size(); i++) {
			//Prüfen, ob Frame gesendet werden muss
			if ((millis() > txFrameBuffer[i].transmitMillis) && ((txFrameBuffer[i].retry <= 1) || (txFrameBuffer[i].syncFlag == true))) {
				//Frame senden
                transmitFrame(txFrameBuffer[i]);
                //Retrys runterzählen
                if (txFrameBuffer[i].retry > 0) {txFrameBuffer[i].retry --;}
                //Nächsten Sendezeitpunkt festlegen (nur relevant, wenn retry > 1)
                txFrameBuffer[i].transmitMillis = millis() + TX_RETRY_TIME + getTOA(30); //Time On Air für Antwort
                //Wenn kein Retry mehr übrig, dann löschen
                if (txFrameBuffer[i].retry == 0) {  
                    //Aus Peer-Liste löschen
                    if (txFrameBuffer[i].initRetry > 1) {availablePeerList(txFrameBuffer[i].viaCall, false);}
                    //Frame löschen
                    txFrameBuffer.erase(txFrameBuffer.begin() + i);
                }
                break;
			}    
		}
	}

    //Senden fertig
    if (irqFlags & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_TX_DONE) {
        radio.startReceive();
        transmittingFlag = false;
        statusTimer = 0;
    }    

    //Daten Empfangen
    if (irqFlags & RADIOLIB_SX127X_CLEAR_IRQ_FLAG_RX_DONE) {
        //Prüfen, ob was empfangen wurde
        uint8_t data[256];
        uint16_t length;
        length = radio.getPacketLength();
        int16_t state = radio.readData(data, length);
        //Empfang wieder starten
        radio.startReceive();
        if (state == RADIOLIB_ERR_NONE) {
            //Empfangene Daten in Frame parsen

            Frame rxFrame;
            rxFrame.importBinary(data, length);

            //In Peer-Liste einfügen
            Peer p;
            //p.call = rxFrame.nodeCall;
            strncpy(p.call, rxFrame.nodeCall.c_str(), sizeof(p.call) - 1); //rxFrame.nodeCall -> p.call

            p.lastRX = time(NULL);
            p.frqError = radio.getFrequencyError();
            p.rssi = radio.getRSSI();
            p.snr = radio.getSNR();
            Serial.printf("main: i.call: %s\n", p.call);
            addPeerList(p);

            //Frame an Monitor senden
            ws.textAll(rxFrame.getMonitorJSON());

            //Frame Typ prüfen & Antwort basteln
            String json = "";
            File file;
            Frame txFrame;
            bool found;
            bool txAbort = false;
            switch (rxFrame.frameType) {
            case Frame::ANNOUNCE:  //Announce 
                //Antowrt zusammenbauen
                if (rxFrame.nodeCall.length() > 0) {
                    txFrame.frameType = Frame::ANNOUNCE_ACK;
                    txFrame.transmitMillis = millis() + ACK_TIME;
                    txFrame.viaCall = rxFrame.nodeCall;
                    txFrameBuffer.push_back(txFrame);
                }
                break;          
            case Frame::ANNOUNCE_ACK:  //Announce REPLAY
                if (rxFrame.viaCall == String(settings.mycall)) {
                    availablePeerList(rxFrame.nodeCall, true);
                }
                break;
            case Frame::MESSAGE_ACK: //Senden abbrechen
                //In Peer-Liste
                if (rxFrame.viaCall == String(settings.mycall)) {
                    availablePeerList(rxFrame.nodeCall, true);
                }
                //Im TX-Puffer nach MSG-ID und NODE-Call suchen und löschen
                for (int i = 0; i < txFrameBuffer.size(); i++) {
                    //ggf. TX-Frames löschen
                    if ((txFrameBuffer[i].id == rxFrame.id) && (txFrameBuffer[i].viaCall == rxFrame.nodeCall)) {
                        //TX Puffer löschen
                        txFrameBuffer.erase(txFrameBuffer.begin() + i); 
                    }
                }
                //ACKs in Datei speichern (für REPEAT und ACK für fremde Frames senden)
                checkACK(rxFrame.srcCall, rxFrame.nodeCall, rxFrame.id);
                //Wenn ich ein ACK direkt bekommen habe, dann extra Eintrag
                if (rxFrame.viaCall == String(settings.mycall)) {
                    checkACK(rxFrame.srcCall, String("direkt"), rxFrame.id);
                }

                break;
            case Frame::TEXT_MESSAGE:  	//TEXT Message
                //Alle "alten" ACKs im TX-Puffer löschen
                bool found = false;
                while (found == true) {
                    found = false;
                    for (int i = 0; i < txFrameBuffer.size(); i++) {
                        if ((txFrameBuffer[i].id == rxFrame.id) && (txFrameBuffer[i].srcCall == rxFrame.srcCall) && (txFrameBuffer[i].frameType == Frame::MESSAGE_ACK)) {
                            txFrameBuffer.erase(txFrameBuffer.begin() + i);
                            found = true;
                            break;
                        }
                    }
                }

                //ACK-Senden bei mir immer, bei anderen nur 1x
                if (((rxFrame.viaCall == String(settings.mycall)) || ((checkACK(rxFrame.srcCall, rxFrame.nodeCall, rxFrame.id) == false) && (checkACK(rxFrame.srcCall, String("direkt"), rxFrame.id) == false))) && (rxFrame.viaCall.length() > 0 )) {
                    Frame r;
                    r.frameType = Frame::MESSAGE_ACK;
                    r.viaCall = rxFrame.nodeCall;
                    r.srcCall = rxFrame.srcCall;
                    r.tx = true;
                    r.id = rxFrame.id ;
                    r.transmitMillis = millis() + ACK_TIME;
                    txFrameBuffer.push_back(r);
                }

                //Message ID und SRC-Call in Datei suchen
                file = LittleFS.open("/messages.json", "r");
                found = false;
                if (file) {
                    JsonDocument doc;
                    while (file.available()) {
                        DeserializationError error = deserializeJson(doc, file);
                        if (error == DeserializationError::Ok) {
                            if ((doc["message"]["id"].as<uint32_t>() == rxFrame.id) && (doc["message"]["srcCall"].as<String>() == rxFrame.srcCall)) {
                                found = true;
                                break; 
                            }
                        } else if (error != DeserializationError::EmptyInput) {
                            file.readStringUntil('\n');
                        }
                    }
                    file.close();                    
                }

                //Message über Websocket senden & speichern
                if (found == false) {
                    //Message über Websocket senden
                    json = rxFrame.getMessageJSON();
                    ws.textAll(json);

                    //Message in Datei speichern
                    file = LittleFS.open("/messages.json", "a"); 
                    if (file) {
                        file.println(json); 
                        file.close();
                        limitFileLines("/messages.json", MAX_STORED_MESSAGES);
                    }

                    //ECHO für Tracking-Message
                    if ((rxFrame.dstCall == String(settings.mycall)) && (rxFrame.messageType == Frame::MessageTypes::TRACE) && (rxFrame.messageText.indexOf("ECHO") == -1)) {
                        String m = "";
                        m = byteArrayToString(rxFrame.message, rxFrame.messageLength);
                        m += " > ECHO ";
                        m += String(settings.mycall);
                        m += " ";
                        m += getFormattedTime("%H:%M:%S");
                        sendTrace(rxFrame.srcCall, m);
                    }

                    //Fernsteuerung
                    if ((rxFrame.dstCall == String(settings.mycall)) && (rxFrame.messageType == Frame::MessageTypes::COMMAND)) {
                        switch (rxFrame.message[0]) {
                            case 0xff: //Firmware
                                sendMessage(rxFrame.srcCall, String(VERSION));
                                break;
                            case 0xfe: //Reboot
                                rebootTimer = millis() + 2500;
                                break;
                        }
                    }


                    //Messages wiederholen
                    if (settings.loraRepeat == true) {
                        //Serial.println ("REPEAT");

                        //Frame vorbereiten
                        Frame f;
                        f.frameType = rxFrame.frameType;
                        f.srcCall = rxFrame.srcCall;
                        f.dstCall = rxFrame.dstCall;
                        f.hopCount = rxFrame.hopCount;
                        if (f.hopCount < 15) {f.hopCount ++;}
                        f.messageType = rxFrame.messageType;
                        memcpy(f.message, rxFrame.message, 255);
                        f.messageLength = rxFrame.messageLength;
                        f.id = rxFrame.id;
                        f.timestamp = time(NULL);
                        f.retry = TX_RETRY;
                        f.initRetry = TX_RETRY;

                        //Prüfen, ob Tracking ein
                        if (rxFrame.messageType == Frame::MessageTypes::TRACE) {
                            //EIN -> Rufzeichen und Uhrzeit dazu
                            String message = byteArrayToString(rxFrame.message, rxFrame.messageLength);
                            message += " > ";
                            message += String(settings.mycall);
                            message += " ";
                            message += getFormattedTime("%H:%M:%S");
                            f.setMessageText(message);
                        } 

                        //Prüfen, an wen man das Frame so senden könnte
                        //ACK-File schon mal auf machen
                        file = LittleFS.open("/ack.json", "r");
                        for (int i = 0; i < peerList.size(); i++) {
                            //Prüfen, ob das Peer das Frame schon mal wiederholt hat
                            found = false;
                            if (file) {
                                JsonDocument doc;
                                file.seek(0);
                                while (file.available()) {
                                    DeserializationError error = deserializeJson(doc, file);
                                    if (error == DeserializationError::Ok) {
                                        if ((doc["id"].as<uint32_t>() == rxFrame.id) && (doc["srcCall"].as<String>() == rxFrame.srcCall) && (doc["nodeCall"].as<String>() == peerList[i].call)) {
                                            found = true;
                                            break; 
                                        }
                                    } else if (error != DeserializationError::EmptyInput) {
                                        file.readStringUntil('\n');
                                    }
                                }
                            }

                            //In TX-Puffer eintragen
                            if ((found == false) && (peerList[i].available == true) && (peerList[i].call != rxFrame.nodeCall)) {
                                //Serial.println("SEND!!!!");
                                f.viaCall = peerList[i].call;
                                txFrameBuffer.push_back(f);
                            }
                        } 
                        file.close();  
                        
                        //Wenn keine Peers da, dann Frame 1x wiederholen
                        // found = false;
                        // for (int i = 0; i < peerList.size(); i++) {
                        //     if ((peerList[i].call =! rxFrame.nodeCall) && (peerList[i].available == true)) {found = true;}
                        // }
                        // if (found == false) {
                        //     f.viaCall = "";
                        //     f.retry = 1;
                        //     f.initRetry = 1;
                        //     txFrameBuffer.push_back(f);
                        // }

                    }
                }
                break;
            }
        }  
    }


    //Status über Websocket senden
    if (millis() > statusTimer) {
        statusTimer = millis() + 1000;
        //Status über Websocket senden
        JsonDocument doc;
        doc["status"]["time"] = time(NULL);
        doc["status"]["tx"] = transmittingFlag;
        doc["status"]["rx"] = receivingFlag;
        doc["status"]["txBufferCount"] = txFrameBuffer.size();
        String jsonOutput;
        serializeJson(doc, jsonOutput);
        ws.textAll(jsonOutput);
    	//Peer-Liste checken
    	checkPeerList();
    }

    //Reboot
    if (millis() > rebootTimer) {ESP.restart();}

}