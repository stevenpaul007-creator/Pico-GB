#pragma once

#include <Arduino.h>
#include "common.h"
#include "SdFat.h"

bool psram_init();
bool psram_read(uint32_t addr, void* buf, size_t len);
bool psram_write(uint32_t addr, const void* buf, size_t len);

// Loads an open FsFile into PSRAM starting at address 0. On success sets out_size to the number of bytes written.
bool load_rom_to_psram(FsFile &f, uint32_t &out_size);

// Return the maximum addressable size of PSRAM in bytes - caller may use to validate ROM sizes
uint32_t psram_size_bytes();

// Convenience: read a single byte from PSRAM
inline uint8_t psram_read8(uint32_t addr) {
  uint8_t v = 0;
  psram_read(addr, &v, 1);
  return v;
}
