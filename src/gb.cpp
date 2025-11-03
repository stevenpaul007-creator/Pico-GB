#include <Arduino.h>

#include "minigb_apu.h"
#include "peanut_gb.h"

#include "gbcolors.h"
#include "gb.h"
#include "common.h"
#if ENABLE_EXT_PSRAM
#include "psram.h"
#endif

struct gb_s gb;
palette_t palette; // Colour palette

uint8_t ram[RAM_SIZE];

// Definition of ROM data
#if !ENABLE_SDCARD
#include "game_bin.h"
const uint8_t *rom = GAME_DATA;
#else

#if ENABLE_RP2040_PSRAM
uint8_t* psram_rom = (uint8_t *)pmalloc(4 * 1024 * 1024); // 4MB max size for RP2040 PSRAM
#endif

/** Definition of ROM data
 * We're going to erase and reprogram the region defines as "Filesystem" in platformio.ini (see board_build.filesystem_size).
 * This is available from _FS_start (i.e. XIP_BASE + program size) to _FS_end. Note that the last sector is reserved for EEPROM.
 * Game Boy DMG ROM size ranges from 32768 bytes (e.g. Tetris) to 1,048,576 bytes (e.g. Pokemod Red)
 */
const uint8_t *rom = (const uint8_t *)(&_FS_start);
#endif

static unsigned char rom_bank0[1024*32];

/**
 * Returns a byte from the ROM file at the given address.
 */
static uint8_t gb_rom_read(struct gb_s* gb, const uint_fast32_t addr) {
  (void)gb;
  
  if (addr < sizeof(rom_bank0))
    return rom_bank0[addr];


#if ENABLE_RP2040_PSRAM
  return psram_rom[addr];
#endif

#if ENABLE_EXT_PSRAM
  // If PSRAM is enabled and rom was loaded into PSRAM, read from PSRAM
  if (addr >= MAX_ROM_SIZE_MB) {
    return psram_read8(addr);
  }
#endif
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

#ifdef USE_BOOT_ROM
#include "boot_cgb_bin.h"
#include "boot_gb_bin.h"
static uint8_t gb_bootrom_read(struct gb_s* gb, const uint_fast16_t addr) {
	if (gb->cgb.cgbMode) {
    return CGB_BOOT_ROM[addr];
  } else {
    return GB_BOOT_ROM[addr];
  }
}
#endif

void initGbContext() {
  
#if ENABLE_RP2040_PSRAM
  memcpy(rom_bank0, psram_rom, sizeof(rom_bank0));
#else
  memcpy(rom_bank0, rom, sizeof(rom_bank0));
#endif

  auto ret = gb_init(&gb, &gb_rom_read, &gb_cart_ram_read, &gb_cart_ram_write, &gb_error, NULL);
  if (ret != GB_INIT_NO_ERROR) {
    error(String("Error initializing emulator: ") + ret);
  }

#ifdef USE_BOOT_ROM
  // gb_bootrom_read has to be set after gb_init(), as it sets it to NULL
  gb_set_bootrom(&gb, &gb_bootrom_read);
  gb_reset(&gb);
#endif
}

void gb_reset(){
  gb_reset(&gb);
}
