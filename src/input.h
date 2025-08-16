#pragma once

#include <PCF8574.h>

/* GPIO Connections. */
#ifdef USE_PAD_GPIO
#define PIN_UP		2
#define PIN_DOWN	3
#define PIN_LEFT	4
#define PIN_RIGHT	5
#define PIN_A		6
#define PIN_B		7
#define PIN_SELECT	8
#define PIN_START	9
#else
#define PIN_UP		0
#define PIN_DOWN	1
#define PIN_LEFT	2
#define PIN_RIGHT	3
#define PIN_A		5
#define PIN_B		4
#define PIN_SELECT	6
#define PIN_START	7
#endif

void initJoypad();

void handlePad();
void handleSerial();
