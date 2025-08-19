#include <Arduino.h>

#include "minigb_apu.h"
#include "peanut_gb.h"

#include "gbcolors.h"
#include "gb.h"
#include "common.h"

struct gb_s gb;
palette_t palette; // Colour palette

uint8_t ram[RAM_SIZE];

// Definition of ROM data
#if !ENABLE_SDCARD
#include "game_bin.h"
const uint8_t *rom = GAME_DATA;
#else
/** Definition of ROM data
 * We're going to erase and reprogram the region defines as "Filesystem" in platformio.ini (see board_build.filesystem_size).
 * This is available from _FS_start (i.e. XIP_BASE + program size) to _FS_end. Note that the last sector is reserved for EEPROM.
 * Game Boy DMG ROM size ranges from 32768 bytes (e.g. Tetris) to 1,048,576 bytes (e.g. Pokemod Red)
 */
const uint8_t *rom = (const uint8_t *)(&_FS_start);
#endif

static unsigned char rom_bank0[65536];

/**
 * Returns a byte from the ROM file at the given address.
 */
static uint8_t gb_rom_read(struct gb_s* gb, const uint_fast32_t addr) {
  (void)gb;
  if (addr < sizeof(rom_bank0))
    return rom_bank0[addr];

  return rom[addr];
}

/**
 * Returns a byte from the cartridge RAM at the given address.
 */
static uint8_t gb_cart_ram_read(struct gb_s* gb, const uint_fast32_t addr) {
  (void)gb;
  return ram[addr];
}

/**
 * Writes a given byte to the cartridge RAM at the given address.
 */
static void gb_cart_ram_write(struct gb_s* gb, const uint_fast32_t addr,
    const uint8_t val) {
  ram[addr] = val;
}

static void gb_error(struct gb_s* gb, const enum gb_error_e gb_err, const uint16_t addr) {
  const char* gb_err_str[4] = {
      "UNKNOWN",
      "INVALID OPCODE",
      "INVALID READ",
      "INVALID WRITE"};
  error(String("Error ") + gb_err + " occurred: " + gb_err_str[gb_err] + " at 0x" + String(addr, 16));
}

gb_init_error_e initGbContext() {
  memcpy(rom_bank0, rom, sizeof(rom_bank0));
  return gb_init(&gb, &gb_rom_read, &gb_cart_ram_read,
      &gb_cart_ram_write, &gb_error, NULL);
}
