#pragma once

#include <stdint.h>
#include "SdFat.h"

#include "common.h"

#define MAX_PATH_LENGTH 256

extern SdFs sd;

void init_sdcard();

void read_cart_ram_file(struct gb_s* gb);

void write_cart_ram_file(struct gb_s* gb);

void rom_file_selector();
