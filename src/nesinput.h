#pragma once
#include "InfoNES.h"
#include "InfoNES_Mapper.h"
#include "InfoNES_System.h"
#include "InfoNES_pAPU.h"
#include "allservices.h"


#define GPLEFT (1 << 6)
#define GPRIGHT (1 << 7)
#define GPUP (1 << 4)
#define GPDOWN (1 << 5)
#define GPSTART (1 << 3)
#define GPSELECT (1 << 2)
#define GPA (1 << 0)
#define GPB (1 << 1)

void initJoypad();
int nesMain();
void nesAudioCallback(void *userdata, int16_t *stream, size_t len);