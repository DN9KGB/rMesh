#pragma once

//Konfiguration
struct Settings {
  bool dhcpActive;
  bool apMode;
  char wifiSSID[64];
  char wifiPassword[64];
  char mycall[17];      // max. 16 Zeichen
  char ntpServer[64];
  IPAddress wifiIP;
  IPAddress wifiNetMask;
  IPAddress wifiGateway;
  IPAddress wifiDNS;
  float loraFrequency;
  int8_t loraOutputPower;
  float loraBandwidth;
  uint8_t loraSyncWord;
  uint8_t loraCodingRate;
  uint8_t loraSpreadingFactor;
  int16_t loraPreambleLength;
  bool loraRepeat;
};

void loadSettings();
void saveSettings();
void showSettings();
void sendSettings();

extern Settings settings;


/*

#include <Arduino.h>
#include "ESPAsyncWebServer.h"





extern Settings settings;
extern AsyncWebSocket ws;

void defaultSettings();
void saveSettings();
bool checkSettings();
void showSettings();
void sendSettings();

*/
