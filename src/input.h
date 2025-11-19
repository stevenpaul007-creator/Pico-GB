#pragma once

#if ENABLE_INPUT == INPUT_PCF8574
#include <PCF8574.h>
#endif

void initJoypad();
bool readJoypad(uint8_t pin);

void handleJoypad();
void handleSerial();
void nextPalette();
