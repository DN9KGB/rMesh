#pragma once
#include <ESPAsyncWebServer.h>

extern AsyncWebSocket ws;

void startWebServer();

/*

#ifndef WEBFUNCTIONS_H
#define WEBFUNCTIONS_H

#include <Arduino.h>
#include "ESPAsyncWebServer.h"


extern AsyncWebSocket ws;
extern uint32_t rebootTimer;
extern uint32_t announceTimer;


String webProcessor(const String& var);

#endif

*/