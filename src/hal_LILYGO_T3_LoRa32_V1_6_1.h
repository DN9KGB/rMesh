#ifndef RF_H
#define RF_H
#include <RadioLib.h>
#include "main.h"



extern bool transmittingFlag;
extern bool receivingFlag;
extern SX1278 radio;


void initRadio();
bool transmitFrame(Frame &f);


//void monitorFrame(Frame &f);




#endif
