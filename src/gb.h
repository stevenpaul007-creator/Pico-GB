#pragma once

#define PEANUT_GB_HEADER_ONLY
#include "peanut_gb.h"

#define GBCOLOR_HEADER_ONLY
#include "gbcolors.h"

extern struct gb_s gb;
extern palette_t palette; // Colour palette
extern const uint8_t* RS_rom;

#if ENABLE_RP2040_PSRAM
extern uint8_t* psram_rom;
#endif

#define GB_RAM_SIZE 32768
extern uint8_t RS_ram[GB_RAM_SIZE];

void initGbContext();
void gb_reset();
