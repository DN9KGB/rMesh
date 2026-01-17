#ifndef HELPER_H
#define HELPER_H

#include <Arduino.h>
#include "frame.h"
#include "main.h"

void sendMessage(String dstCall, String text);
void sendTrace(String dstCall, String text);
void sendFrame(Frame &f);
String getFormattedTime(const char* format);
void limitFileLines(const char* path, int maxLines);
void printHexArray(uint8_t* data, size_t length);
String byteArrayToString(uint8_t* buffer, size_t length);
String ackJson(String srcCall, String nodeCall, uint32_t id);
bool checkACK(String srcCall, String nodeCall, uint32_t id);
uint32_t getTOA(uint8_t payloadBytes);
void addSourceCall(uint8_t* data, uint8_t &len);
void sendPeerList();
void addPeerList(Peer p);
void checkPeerList();
void availablePeerList(String call, bool available);

#endif