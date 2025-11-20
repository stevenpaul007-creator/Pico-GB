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

// 状态文件头部结构
struct gb_save_state_s {

	/* Cartridge information:
	 * Memory Bank Controller (MBC) type. */
	int8_t mbc;
	/* Whether the MBC has internal RAM. */
	uint8_t cart_ram;
	/* Number of ROM banks in cartridge. */
	uint16_t num_rom_banks_mask;
	/* Number of RAM banks in cartridge. Ignore for MBC2. */
	uint8_t num_ram_banks;

	uint16_t selected_rom_bank;
	/* WRAM and VRAM bank selection not available. */
	uint8_t cart_ram_bank;
	uint8_t enable_cart_ram;
	/* Cartridge ROM/RAM mode select. */
	uint8_t cart_mode_select;

	union cart_rtc rtc_latched, rtc_real;

	struct cpu_registers_s cpu_reg;
	struct count_s counter;

	uint8_t wram[WRAM_SIZE];
	uint8_t vram[VRAM_SIZE];
	uint8_t oam[OAM_SIZE];
	uint8_t hram_io[HRAM_IO_SIZE];
	
#if PEANUT_FULL_GBC_SUPPORT
	/* Game Boy Color Mode*/
	struct {
		uint8_t cgbMode;
		uint8_t doubleSpeed;
		uint8_t doubleSpeedPrep;
		uint8_t wramBank;
		uint16_t wramBankOffset;
		uint8_t vramBank;
		uint16_t vramBankOffset;
		uint16_t fixPalette[0x40];  //BG then OAM palettes fixed for the screen
		uint8_t OAMPalette[0x40];
		uint8_t BGPalette[0x40];
		uint8_t OAMPaletteID;
		uint8_t BGPaletteID;
		uint8_t OAMPaletteInc;
		uint8_t BGPaletteInc;
		uint8_t dmaActive;
		uint8_t dmaMode;
		uint8_t dmaSize;
		uint16_t dmaSource;
		uint16_t dmaDest;
	} cgb;
#endif
};

void save_state(struct gb_s* gb);
void load_state(struct gb_s* gb);