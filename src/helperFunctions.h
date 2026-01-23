#pragma once

#include <Arduino.h>

#include "frame.h"

void printHexArray(uint8_t* data, size_t length);
void addJSONtoFile(char* buffer, size_t length, const char* file, const uint16_t lines);
uint32_t getTOA(uint8_t payloadBytes);
void availablePeerList(const char* call, bool available);
void sendMessage(const char* dstCall, const char* text, uint8_t messageType = Frame::MessageTypes::TEXT_MESSAGE); 
void sendPeerList();
bool checkACK(const char* srcCall, const char* nodeCall, const uint32_t id);
void safeUtf8Copy(char* dest, const uint8_t* src, size_t maxLength);
void addPeerList(Frame &f);
void checkPeerList();
void getFormattedTime(const char* format, char* outBuffer, size_t outSize);

