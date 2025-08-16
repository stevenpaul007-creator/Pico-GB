/**
 * Copyright (C) 2022 by Mahyar Koshkouei <mk@deltabeard.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "gb.h"

/* C Headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RP2040 Headers */
#include <hardware/vreg.h>

/* Project headers */
#include "hedley.h"
#include "minigb_apu.h"

// #include "sdcard.h"
#include "common.h"
#include "game_bin.h"
#include "i2s.h"

#define GBCOLOR_HEADER_ONLY
#include "gbcolors.h"

#include "input.h"

#if ENABLE_SOUND
/**
 * Global variables for audio task
 * stream contains N=AUDIO_SAMPLES samples
 * each sample is 32 bits
 * 16 bits for the left channel + 16 bits for the right channel in stereo interleaved format)
 * This is intended to be played at AUDIO_SAMPLE_RATE Hz
 */
uint16_t* stream;
#endif

/** Definition of ROM data
 * We're going to erase and reprogram a region 1Mb from the start of the flash
 * Once done, we can access this at XIP_BASE + 1Mb.
 * Game Boy DMG ROM size ranges from 32768 bytes (e.g. Tetris) to 1,048,576 bytes (e.g. Pokemod Red)
 */
// #define FLASH_TARGET_OFFSET (1024 * 1024)
// const uint8_t *rom = (const uint8_t *)(XIP_BASE + FLASH_TARGET_OFFSET);
const uint8_t* rom = GAME_DATA;
static unsigned char rom_bank0[65536];

static uint8_t ram[32768];
static uint8_t manual_palette_selected = 0;

struct gb_s gb;
palette_t palette; // Colour palette

uint_fast32_t frames = 0;

#define putstdio(x) write(1, x, strlen(x))

/**
 * Returns a byte from the ROM file at the given address.
 */
uint8_t gb_rom_read(struct gb_s* gb, const uint_fast32_t addr) {
  (void)gb;
  if (addr < sizeof(rom_bank0))
    return rom_bank0[addr];

  return rom[addr];
}

/**
 * Returns a byte from the cartridge RAM at the given address.
 */
uint8_t gb_cart_ram_read(struct gb_s* gb, const uint_fast32_t addr) {
  (void)gb;
  return ram[addr];
}

/**
 * Writes a given byte to the cartridge RAM at the given address.
 */
void gb_cart_ram_write(struct gb_s* gb, const uint_fast32_t addr,
    const uint8_t val) {
  ram[addr] = val;
}

/**
 * Ignore all errors.
 */
void gb_error(struct gb_s* gb, const enum gb_error_e gb_err, const uint16_t addr) {
#if 1
  const char* gb_err_str[4] = {
      "UNKNOWN",
      "INVALID OPCODE",
      "INVALID READ",
      "INVALID WRITE"};
  Serial.printf("Error %d occurred: %s at %04X\n.\n", gb_err, gb_err_str[gb_err], addr);
//	abort();
#endif
}

void startEmulator() {
#if ENABLE_LCD
#if ENABLE_SDCARD
  /* ROM File selector */
  lcd_init();
  tft.fillScreen(TFT_BLACK);
  rom_file_selector();
#endif
#endif

  /* Initialise GB context. */
  memcpy(rom_bank0, rom, sizeof(rom_bank0));
  auto ret = gb_init(&gb, &gb_rom_read, &gb_cart_ram_read,
      &gb_cart_ram_write, &gb_error, NULL);
  Serial.println("GB ");

  if (ret != GB_INIT_NO_ERROR) {
    Serial.printf("Error: %d\n", ret);
    reset();
  }

  /* Automatically assign a colour palette to the game */
  char rom_title[16];
  auto_assign_palette(palette, gb_colour_hash(&gb), gb_get_rom_name(&gb, rom_title));

#if ENABLE_LCD
  gb_init_lcd(&gb, &lcd_draw_line);

  /* Start Core1, which processes requests to the LCD. */
  Serial.println("CORE1 ");
  multicore_launch_core1(core1_init);

  Serial.println("LCD ");
#endif

#if ENABLE_SOUND
  // Initialize audio emulation
  audio_init();

  Serial.println("AUDIO ");
#endif

#if ENABLE_SDCARD
  /* Load Save File. */
  read_cart_ram_file(&gb);
#endif

  Serial.print("\n> ");
}

void reset() {
  Serial.println("\nEmulation Ended");
  /* stop lcd task running on core 1 */
  multicore_reset_core1();
  watchdog_reboot(0,0,0);
}

void overclock() {
  const unsigned vco = 1596 * 1000 * 1000; /* 266MHz */
  const unsigned div1 = 6, div2 = 1;

  vreg_set_voltage(VREG_VOLTAGE_1_15);
  sleep_ms(2);
  set_sys_clock_pll(vco, div1, div2);
  sleep_ms(2);
}

void setup() {
  overclock();

  /* Initialise USB serial connection for debugging. */
  Serial.begin(115200);
  //while (!Serial) ;
  //delay(2000);

#if ENABLE_SDCARD
  time_init();
#endif
  // sleep_ms(5000);
  Serial.println("INIT: ");

  /* Initialise joypad. */
  initJoypad();

/* Set SPI clock to use high frequency. */
#if 0
	clock_configure(clk_peri, 0,
			CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS,
			125 * 1000 * 1000, 125 * 1000 * 1000);
	spi_init(spi0, 30*1000*1000);
	spi_set_format(spi0, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
#endif

#if ENABLE_SOUND
  // Allocate memory for the stream buffer
  stream = malloc(AUDIO_BUFFER_SIZE_BYTES);
  assert(stream != NULL);
  memset(stream, 0, AUDIO_BUFFER_SIZE_BYTES); // Zero out the stream buffer

  // Initialize I2S sound driver
  i2s_config_t i2s_config = i2s_get_default_config();
  i2s_config.sample_freq = AUDIO_SAMPLE_RATE;
  i2s_config.dma_trans_count = AUDIO_SAMPLES;
  i2s_volume(&i2s_config, 2);
  i2s_init(&i2s_config);
#endif

  startEmulator();
}

void nextPalette() {
  manual_palette_selected = manual_palette_selected < 12 ? manual_palette_selected + 1 : 0;
  manual_assign_palette(palette, manual_palette_selected);
}

void prevPalette() {
  manual_palette_selected = manual_palette_selected > 0 ? manual_palette_selected - 1 : 12;
  manual_assign_palette(palette, manual_palette_selected);
}

void loop() {
  gb.gb_frame = 0;

  do {
    __gb_step_cpu(&gb);
    tight_loop_contents();
  } while (HEDLEY_LIKELY(gb.gb_frame == 0));

  frames++;
#if ENABLE_SOUND
  if (!gb.direct.frame_skip) {
    audio_callback(NULL, stream, AUDIO_BUFFER_SIZE_BYTES);
    i2s_dma_write(&i2s_config, stream);
  }
#endif

  handlePad();
  handleSerial();
}
