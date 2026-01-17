#include "serial.h"
#include <Arduino.h>
#include "main.h"
#include "settings.h"
#include <LittleFS.h>
#include <WiFi.h>
#include "webFunctions.h"
#include "wifiFunctions.h"

String serialRxBuffer;
//uint8_t serialrxBufferLength = 0;

void checkSerialRX() {
    if (Serial.available() > 0) {
        char rx = Serial.read();
        //Echo
        Serial.write(rx);
        if ((rx == 13) || (rx == 10)) {
            //Auswerten
            if (serialRxBuffer.length() > 0 ) {
                //Befehl & Parameter auseinanderwurschteln
                String parameter = "";
                if (serialRxBuffer.indexOf(" ") > 0) {
                    //Leerzeichen gefunden -> Befehl ausschneiden
                    parameter = serialRxBuffer.substring(serialRxBuffer.indexOf(" ") + 1);
                    serialRxBuffer = serialRxBuffer.substring(0, serialRxBuffer.indexOf(" "));
                }   
                serialRxBuffer.toLowerCase();
                Serial.println();
                // Serial.printf("Befehl: ***%s***\n", serialRxBuffer);
                // Serial.printf("Parameter: ***%s***\n", parameter);
                
                //Befehle auswerten
                //Hilfe
                if (serialRxBuffer.substring(0, 1) == "h") {
                    File file = LittleFS.open("/help.txt", "r");
                    if (file) {
                        while (file.available()) {
                            String line = file.readStringUntil('\n');
                            line.replace("\r", "");
                            Serial.println(line);
                        }
                        file.close(); 
                    }
                }

                //Version
                if (serialRxBuffer.substring(0, 1) == "v") {
                    Serial.println(String(VERSION));
                }

                //Testfunktionen
                if (serialRxBuffer.substring(0, 1) == "t") {
                    Frame f;
                    f.frameType = Frame::FrameType::TEXT_MESSAGE;
                    f.messageType = Frame::MessageTypes::TRACE;
                    uint8_t daten[10] = {0};
                    memcpy(f.message, daten, 10);
                    f.messageLength = 1;
                    txFrameBuffer.push_back(f);
                }

                //Reboot
                if (serialRxBuffer.substring(0, 1) == "r") {
                    Serial.println("Reboot...");
                    rebootTimer = millis() + 500;
                }

                //Settings
                if (serialRxBuffer.substring(0, 2) == "se") {
                    showSettings();
                }

                //Wifi Scannen
                if (serialRxBuffer.substring(0, 2) == "sc") {
                    Serial.println("WiFi scan.....");
                    WiFi.scanNetworks(true);
                }

                //Wifi SSID
                if (serialRxBuffer.substring(0, 2) == "ss") {
                    if (parameter.length() > 0) {
                        parameter.toCharArray(settings.wifiSSID, sizeof(settings.wifiSSID));
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("WiFi SSID: %s\n", settings.wifiSSID);
                }

                //Wifi Password
                if (serialRxBuffer.substring(0, 1) == "p") {
                    if (parameter.length() > 0) {
                        parameter.toCharArray(settings.wifiPassword, sizeof(settings.wifiPassword));
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("WiFi Passwort: %s\n", settings.wifiPassword);
                }
                
                //IP-Adresse
                if (serialRxBuffer.substring(0, 1) == "i") {
                    if (parameter.length() > 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter)) {
                            settings.wifiIP = tempIP;
                            saveSettings();
                            wifiInit();
                        } else {
                            Serial.println("Fehler: Ungültiges IP-Format!");
                        }                    
                    }
                    Serial.printf("IP: %d.%d.%d.%d\n", settings.wifiIP[0], settings.wifiIP[1], settings.wifiIP[2], settings.wifiIP[3]);
                }

                //Gateway
                if (serialRxBuffer.substring(0, 1) == "g") {
                    if (parameter.length() > 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter)) {
                            settings.wifiGateway = tempIP;
                            saveSettings();
                            wifiInit();
                        } else {
                            Serial.println("Fehler: Ungültiges IP-Format!");
                        }                    
                    }
                    Serial.printf("Gateway: %d.%d.%d.%d\n", settings.wifiGateway[0], settings.wifiGateway[1], settings.wifiGateway[2], settings.wifiGateway[3]);
                }

                //DNS-Adresse
                if (serialRxBuffer.substring(0, 2) == "dn") {
                    if (parameter.length() > 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter)) {
                            settings.wifiDNS = tempIP;
                            saveSettings();
                            wifiInit();
                        } else {
                            Serial.println("Fehler: Ungültiges IP-Format!");
                        }                    
                    }
                    Serial.printf("DNS: %d.%d.%d.%d\n", settings.wifiDNS[0], settings.wifiDNS[1], settings.wifiDNS[2], settings.wifiDNS[3]);
                }

                //Netmask
                if (serialRxBuffer.substring(0, 2) == "ne") {
                    if (parameter.length() > 0) {
                        IPAddress tempIP;
                        if (tempIP.fromString(parameter)) {
                            settings.wifiNetMask = tempIP;
                            saveSettings();
                            wifiInit();
                        } else {
                            Serial.println("Fehler: Ungültiges IP-Format!");
                        }                    
                    }
                    Serial.printf("Netmask: %d.%d.%d.%d\n", settings.wifiNetMask[0], settings.wifiNetMask[1], settings.wifiNetMask[2], settings.wifiNetMask[3]);
                }

                //AP-Mode
                if (serialRxBuffer.substring(0, 1) == "a") {
                    if (parameter.length() > 0) {
                        parameter = parameter.substring(0,1);
                        bool value = false;
                        if ((parameter == "1") || (parameter == "e") || (parameter == "t")) {value = true;}
                        settings.apMode = value;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("AP-Mode: %s\n", settings.apMode ? "true" : "false");
                }
                
                //DHCP-Mode
                if (serialRxBuffer.substring(0, 1) == "d") {
                    if (parameter.length() > 0) {
                        parameter = parameter.substring(0,1);
                        bool value = false;
                        if ((parameter == "1") || (parameter == "e") || (parameter == "t")) {value = true;}
                        settings.dhcpActive = value;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("DHCP: %s\n", settings.dhcpActive ? "true" : "false");
                }                
                
            }
            //Puffer löschen
            serialRxBuffer = "";
        } else {
            //RX-Byte in Puffer
            if (serialRxBuffer.length() < MAX_SERIAL_BUFFER_LENGTH) {
                serialRxBuffer.concat(String(rx));
            }
        }
    }
}

