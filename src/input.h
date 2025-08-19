#pragma once

#include <PCF8574.h>

void initJoypad();
bool readJoypad(uint8_t pin);

void handleJoypad();
void handleSerial();
