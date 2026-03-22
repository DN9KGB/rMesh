#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <cstring>
#include <nvs_flash.h>

#include "frame.h"
#include "serial.h"
#include "config.h"
#include "settings.h"
#include "hal.h"
#include "settings.h"
#include "main.h"
#include "wifiFunctions.h"
#include "helperFunctions.h"
#include "routing.h"

//String serialRxBuffer;

char serialRxBuffer[200] = {0}; 


void checkSerialRX() {
    if (Serial.available() > 0) {
        char rx = Serial.read();
        //Echo
        Serial.write(rx);
        if ((rx == 13) || (rx == 10)) {
            //Auswerten
            if (strlen(serialRxBuffer) > 0 ) {

                //Parameter kopieren
                char parameter[200] = {0};
                char* pos = strchr(serialRxBuffer, ' ');
                if (pos != nullptr) {
                    pos++; // hinter das Leerzeichen
                    strncpy(parameter, pos, sizeof(parameter) - 1); // sicheren Copy
                    Serial.println(parameter);
                }

                //Puffer nach Kleinbuchstaben
                for (int i = 0; serialRxBuffer[i] != '\0'; i++) {
                    serialRxBuffer[i] = tolower(serialRxBuffer[i]);
                }
                
                //Befehle auswerten

                //Testfunktionen
                if (strncmp(serialRxBuffer, "t", 1) == 0) {
                    
           struct tm tm;
    tm.tm_year = 2024 - 1900; // Jahr seit 1900
    tm.tm_mon = 1;            // Februar (0-11, also 1 = Februar)
    tm.tm_mday = 14;          // Tag
    tm.tm_hour = 4;           // Stunde
    tm.tm_min = 59;           // Minute
    tm.tm_sec = 0;            // Sekunde
    
    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, NULL);
    
    Serial.println("Uhrzeit manuell auf 03:59:00 gesetzt!");         

                }


                //Hilfe
                if (strncmp(serialRxBuffer, "h", 1) == 0) {
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
                if (strncmp(serialRxBuffer, "v", 1) == 0) {
                    //+ BOARD TYPE
                    Serial.printf("\n\n\n%s\n%s %s\nREADY.\n", PIO_ENV_NAME, NAME, VERSION);   
                }

                //Settings
                if (strncmp(serialRxBuffer, "se", 2) == 0) {
                    showSettings();
                }

                //Reboot
                if (serialRxBuffer[0] == 'r' && (serialRxBuffer[1] == ' ' || serialRxBuffer[1] == '\0')) {
                    Serial.println("Reboot...");
                    rebootTimer = 0;
                }

                //Wifi Scannen
                if (strncmp(serialRxBuffer, "sc", 2) == 0) {
                    Serial.println("WiFi scan.....");
                    WiFi.scanNetworks(true);
                }

                //Wifi SSID
                if (strncmp(serialRxBuffer, "ss", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.wifiSSID, parameter, sizeof(settings.wifiSSID) - 1);
                        settings.wifiSSID[sizeof(settings.wifiSSID) - 1] = '\0'; 
                        settings.apMode = false;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("WiFi SSID: %s\n", settings.wifiSSID);
                }    
                
                //Wifi Password
                if (serialRxBuffer[0] == 'p' && (serialRxBuffer[1] == ' ' || serialRxBuffer[1] == '\0')) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.wifiPassword, parameter, sizeof(settings.wifiPassword) - 1);
                        settings.wifiPassword[sizeof(settings.wifiPassword) - 1] = '\0'; 
                        settings.apMode = false;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("WiFi Passwort: %s\n", settings.wifiPassword);
                }            
                
                //IP-Adresse
                if (strncmp(serialRxBuffer, "i", 1) == 0) {
                    if (strlen(parameter) > 0) {
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
                if (strncmp(serialRxBuffer, "g", 1) == 0) {
                    if (strlen(parameter) > 0) {
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
                if (strncmp(serialRxBuffer, "dn", 2) == 0) {
                    if (strlen(parameter) > 0) {
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
                if (strncmp(serialRxBuffer, "ne", 2) == 0) {
                    if (strlen(parameter) > 0) {
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
                if (strncmp(serialRxBuffer, "a", 1) == 0) {
                    if (strlen(parameter) > 0) {
                        bool value = false;
                        if (parameter[0] == '1') value = true;
                        if (parameter[0] == 'e') value = true;
                        if (parameter[0] == 't') value = true;
                        settings.apMode = value;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("AP-Mode: %s\n", settings.apMode ? "true" : "false");
                }

                //DHCP-Mode
                if (strncmp(serialRxBuffer, "dh", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        bool value = false;
                        if (parameter[0] == '1') value = true;
                        if (parameter[0] == 'e') value = true;
                        if (parameter[0] == 't') value = true;
                        settings.dhcpActive = value;
                        saveSettings();
                        wifiInit();
                    }
                    Serial.printf("DHCP: %s\n", settings.dhcpActive ? "true" : "false");
                } 

                //Defaults
                if (strncmp(serialRxBuffer, "de", 2) == 0) {
                    std::memset(settings.mycall, 0xff, sizeof(settings.mycall));
                    nvs_flash_erase(); // Löscht die gesamte NVS-Partition
                    nvs_flash_init();  // Initialisiert sie neu
                    rebootTimer = 0;
                }

                // Frequenz-Preset
                // "freq 433" → 433-MHz-Amateurfunk-Preset
                // "freq 868" → 868-MHz-Public-Preset (Sub-Band P, 500 mW)
                if (strncmp(serialRxBuffer, "fr", 2) == 0) {
                    if (strncmp(parameter, "433", 3) == 0) {
                        settings.loraFrequency       = 434.850f;
                        settings.loraBandwidth       = 62.5f;
                        settings.loraSpreadingFactor = 7;
                        settings.loraCodingRate      = 6;
                        settings.loraOutputPower     = 20;
                        settings.loraPreambleLength  = 10;
                        settings.loraSyncWord        = syncWordForFrequency(settings.loraFrequency);
                        saveSettings();
                        Serial.println("Preset 433 MHz (Amateurfunk) gesetzt.");
                    } else if (strncmp(parameter, "868", 3) == 0) {
                        settings.loraFrequency       = 869.525f;
                        settings.loraBandwidth       = 125.0f;
                        settings.loraSpreadingFactor = 7;
                        settings.loraCodingRate      = 5;
                        settings.loraOutputPower     = 27;
                        settings.loraPreambleLength  = 10;
                        settings.loraSyncWord        = syncWordForFrequency(settings.loraFrequency);
                        saveSettings();
                        Serial.println("Preset 868 MHz (Public, 500 mW) gesetzt.");
                    } else {
                        Serial.printf("Aktuelle Frequenz: %.3f MHz\n", settings.loraFrequency);
                        Serial.println("Verwendung: freq 433  oder  freq 868");
                    }
                }


                // Callsign
                // "call DG2NBN-1" → Callsign setzen
                if (strncmp(serialRxBuffer, "call", 4) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.mycall, parameter, sizeof(settings.mycall) - 1);
                        settings.mycall[sizeof(settings.mycall) - 1] = '\0';
                        for (size_t i = 0; settings.mycall[i]; i++) settings.mycall[i] = toupper(settings.mycall[i]);
                        saveSettings();
                    }
                    Serial.printf("Callsign: %s\n", settings.mycall);
                }

                // Position
                // "pos JN48mw" oder "pos 48.1234,11.5678"
                if (strncmp(serialRxBuffer, "pos", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.position, parameter, sizeof(settings.position) - 1);
                        settings.position[sizeof(settings.position) - 1] = '\0';
                        saveSettings();
                    }
                    Serial.printf("Position: %s\n", settings.position);
                }

                // NTP-Server
                if (strncmp(serialRxBuffer, "ntp", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        strncpy(settings.ntpServer, parameter, sizeof(settings.ntpServer) - 1);
                        settings.ntpServer[sizeof(settings.ntpServer) - 1] = '\0';
                        saveSettings();
                    }
                    Serial.printf("NTP: %s\n", settings.ntpServer);
                }

                // TX-Power (Output Power in dBm)
                // "op 20" → TX-Power auf 20 dBm setzen
                if (strncmp(serialRxBuffer, "op", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        int8_t txp = (int8_t)atoi(parameter);
                        if (isPublicBand(settings.loraFrequency) && txp > PUBLIC_MAX_TX_POWER) { txp = PUBLIC_MAX_TX_POWER; }
                        settings.loraOutputPower = txp;
                        saveSettings();
                    }
                    Serial.printf("TX Power: %d dBm\n", settings.loraOutputPower);
                }

                // Bandwidth in kHz
                // "bw 62.5"
                if (strncmp(serialRxBuffer, "bw", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraBandwidth = atof(parameter);
                        saveSettings();
                    }
                    Serial.printf("Bandwidth: %.2f kHz\n", settings.loraBandwidth);
                }

                // Spreading Factor (6–12)
                if (strncmp(serialRxBuffer, "sf", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraSpreadingFactor = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("Spreading Factor: %d\n", settings.loraSpreadingFactor);
                }

                // Coding Rate (5–8)
                if (strncmp(serialRxBuffer, "cr", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraCodingRate = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("Coding Rate: %d\n", settings.loraCodingRate);
                }

                // Preamble Length
                // "pl 10"
                if (strncmp(serialRxBuffer, "pl", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraPreambleLength = (int16_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("Preamble Length: %d\n", settings.loraPreambleLength);
                }

                // Sync Word (hexadezimal, z.B. "sw 2B")
                if (strncmp(serialRxBuffer, "sw", 2) == 0) {
                    if (strlen(parameter) > 0) {
                        settings.loraSyncWord = (uint8_t)strtol(parameter, nullptr, 16);
                        saveSettings();
                    }
                    Serial.printf("SyncWord: %02X\n", settings.loraSyncWord);
                }

                // Repeat / Relay
                // "rep 1" oder "rep 0"
                if (strncmp(serialRxBuffer, "rep", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        bool value = (parameter[0] == '1' || parameter[0] == 'e' || parameter[0] == 't');
                        settings.loraRepeat = value;
                        saveSettings();
                    }
                    Serial.printf("Repeat: %s\n", settings.loraRepeat ? "true" : "false");
                }

                // Max Hop Message
                if (strncmp(serialRxBuffer, "mhm", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        extSettings.maxHopMessage = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("MaxHopMessage: %d\n", extSettings.maxHopMessage);
                }

                // Max Hop Position
                if (strncmp(serialRxBuffer, "mhp", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        extSettings.maxHopPosition = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("MaxHopPosition: %d\n", extSettings.maxHopPosition);
                }

                // Max Hop Telemetry
                if (strncmp(serialRxBuffer, "mht", 3) == 0) {
                    if (strlen(parameter) > 0) {
                        extSettings.maxHopTelemetry = (uint8_t)atoi(parameter);
                        saveSettings();
                    }
                    Serial.printf("MaxHopTelemetry: %d\n", extSettings.maxHopTelemetry);
                }

                // UDP-Peer
                // "udp 2 192.168.1.1" → Peer 2 (1-basiert) auf IP setzen
                // "udp"               → alle Peers anzeigen
                if (strncmp(serialRxBuffer, "udp", 3) == 0) {
                    char* spacePos = strchr(parameter, ' ');
                    if (spacePos != nullptr) {
                        int idx = atoi(parameter) - 1; // 1-basiert → 0-basiert
                        if (idx >= 0 && idx < 5) {
                            spacePos++;
                            IPAddress tempIP;
                            if (tempIP.fromString(spacePos)) {
                                extSettings.udpPeer[idx] = tempIP;
                                saveSettings();
                                Serial.printf("UDP-Peer %d: %d.%d.%d.%d\n", idx + 1, tempIP[0], tempIP[1], tempIP[2], tempIP[3]);
                            } else {
                                Serial.println("Fehler: Ungültiges IP-Format!");
                            }
                        } else {
                            Serial.println("Fehler: Index 1–5 erwartet!");
                        }
                    } else {
                        for (int i = 0; i < 5; i++) {
                            Serial.printf("UDP-Peer %d: %d.%d.%d.%d\n", i + 1, extSettings.udpPeer[i][0], extSettings.udpPeer[i][1], extSettings.udpPeer[i][2], extSettings.udpPeer[i][3]);
                        }
                    }
                }

            }
            //Puffer löschen
            serialRxBuffer[0] = '\0';
        } else {
            //RX-Byte in Puffer
            size_t len = strlen(serialRxBuffer);
            if (len < sizeof(serialRxBuffer) - 1) {
                serialRxBuffer[len] = rx;
                serialRxBuffer[len + 1] = '\0';
            }

        }
    }
    
}

