#pragma once

#include "frame.h"

void initUDP();

bool checkUDP(Frame &f);
void sendUDP(Frame &f);