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

// Peanut-GB emulator settings
#define ENABLE_LCD 1
#define ENABLE_SOUND 0 // 1
#define ENABLE_SDCARD 0 // 1
#define PEANUT_GB_HIGH_LCD_ACCURACY 1
#define PEANUT_GB_USE_BIOS 0
#include "peanut_gb.h"

/**
 * Reducing VSYNC calculation to lower multiple.
 * When setting a clock IRQ to DMG_CLOCK_FREQ_REDUCED, count to
 * SCREEN_REFRESH_CYCLES_REDUCED to obtain the time required each VSYNC.
 * DMG_CLOCK_FREQ_REDUCED = 2^18, and SCREEN_REFRESH_CYCLES_REDUCED = 4389.
 * Currently unused.
 */
#define VSYNC_REDUCTION_FACTOR 16u
#define SCREEN_REFRESH_CYCLES_REDUCED (SCREEN_REFRESH_CYCLES / VSYNC_REDUCTION_FACTOR)
#define DMG_CLOCK_FREQ_REDUCED (DMG_CLOCK_FREQ / VSYNC_REDUCTION_FACTOR)

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
#include "core.h"
#include "game_bin.h"
#include "i2s.h"

/* GPIO Connections. */
#define GPIO_UP 16
#define GPIO_DOWN 16
#define GPIO_LEFT 16
#define GPIO_RIGHT 16
#define GPIO_A 16
#define GPIO_B 16
#define GPIO_SELECT 16
#define GPIO_START 16

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

static struct
{
  unsigned a : 1;
  unsigned b : 1;
  unsigned select : 1;
  unsigned start : 1;
  unsigned right : 1;
  unsigned left : 1;
  unsigned up : 1;
  unsigned down : 1;
} prev_joypad_bits;

static struct gb_s gb;

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

void stop();

void start() {
#if ENABLE_LCD
#if ENABLE_SDCARD
  /* ROM File selector */
  lcd_init();
  lcd_fill(TFT_BLACK); // 0x0000);
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
    stop();
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

void stop() {
  Serial.println("\nEmulation Ended");
  /* stop lcd task running on core 1 */
  multicore_reset_core1();
  while (true)
    ;
}

void setup(void) {
  /* Overclock. */
  {
    const unsigned vco = 1596 * 1000 * 1000; /* 266MHz */
    const unsigned div1 = 6, div2 = 1;

    vreg_set_voltage(VREG_VOLTAGE_1_15);
    sleep_ms(2);
    set_sys_clock_pll(vco, div1, div2);
    sleep_ms(2);
  }

  /* Initialise USB serial connection for debugging. */
  Serial.begin(115200);
  while (!Serial)
    ;

#if ENABLE_SDCARD
  time_init();
#endif
  // sleep_ms(5000);
  Serial.println("INIT: ");

  /* Initialise GPIO pins. */
  gpio_set_function(GPIO_UP, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_DOWN, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_LEFT, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_RIGHT, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_A, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_B, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_SELECT, GPIO_FUNC_SIO);
  gpio_set_function(GPIO_START, GPIO_FUNC_SIO);

  gpio_set_dir(GPIO_UP, false);
  gpio_set_dir(GPIO_DOWN, false);
  gpio_set_dir(GPIO_LEFT, false);
  gpio_set_dir(GPIO_RIGHT, false);
  gpio_set_dir(GPIO_A, false);
  gpio_set_dir(GPIO_B, false);
  gpio_set_dir(GPIO_SELECT, false);
  gpio_set_dir(GPIO_START, false);

  gpio_pull_up(GPIO_UP);
  gpio_pull_up(GPIO_DOWN);
  gpio_pull_up(GPIO_LEFT);
  gpio_pull_up(GPIO_RIGHT);
  gpio_pull_up(GPIO_A);
  gpio_pull_up(GPIO_B);
  gpio_pull_up(GPIO_SELECT);
  gpio_pull_up(GPIO_START);

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

  start();
}

void nextPalette() {
  manual_palette_selected = manual_palette_selected < 12 ? manual_palette_selected + 1 : 0;
  manual_assign_palette(palette, manual_palette_selected);
}

void prevPalette() {
  manual_palette_selected = manual_palette_selected > 0 ? manual_palette_selected - 1 : 12;
  manual_assign_palette(palette, manual_palette_selected);
}

void handleInput(uint_fast32_t& frames) {
  static uint64_t start_time = time_us_64();

  /* Serial monitor commands */
  int input = Serial.read();
  if (input <= 0) {
    return;
  }

  switch (input) {
#if 0
    static bool invert = false;
    static bool sleep = false;
    static uint8_t freq = 1;
    static ili9225_color_mode_e colour = ILI9225_COLOR_MODE_FULL;

    case 'i':
        invert = !invert;
        mk_ili9225_display_control(invert, colour);
        break;

    case 'f':
        freq++;
        freq &= 0x0F;
        mk_ili9225_set_drive_freq(freq);
        Serial.printf("Freq %u\n", freq);
        break;
#endif
  case 'c': {
#if 0
        static ili9225_color_mode_e mode = ILI9225_COLOR_MODE_FULL;
        union core_cmd cmd;

        mode = !mode;
        cmd.cmd = CORE_CMD_IDLE_SET;
        cmd.data = mode;
        multicore_fifo_push_blocking(cmd.full);
#endif
    break;
  }

  case 'i':
    gb.direct.interlace = !gb.direct.interlace;
    break;

  case 'f':
    gb.direct.frame_skip = !gb.direct.frame_skip;
    break;

  case 'b': {
    uint64_t end_time;
    uint32_t diff;
    uint32_t fps;

    end_time = time_us_64();
    diff = end_time - start_time;
    fps = ((uint64_t)frames * 1000 * 1000) / diff;
    Serial.printf("Frames: %u\n"
                  "Time: %lu us\n"
                  "FPS: %lu\n",
        frames, diff, fps);
    Serial.flush();
    frames = 0;
    start_time = time_us_64();
    break;
  }

  case '\n':
  case '\r': {
    gb.direct.joypad_bits.start = 0;
    break;
  }

  case '\b': {
    gb.direct.joypad_bits.select = 0;
    break;
  }

  case '8': {
    gb.direct.joypad_bits.up = 0;
    break;
  }

  case '2': {
    gb.direct.joypad_bits.down = 0;
    break;
  }

  case '4': {
    gb.direct.joypad_bits.left = 0;
    break;
  }

  case '6': {
    gb.direct.joypad_bits.right = 0;
    break;
  }

  case 'z':
  case 'w': {
    gb.direct.joypad_bits.a = 0;
    break;
  }

  case 'x': {
    gb.direct.joypad_bits.b = 0;
    break;
  }

  case 'q':
    stop();

  case 'p':
    nextPalette();

  default:
    break;
  }
}

void handlePad() {
  /* Update buttons state */
  prev_joypad_bits.up = gb.direct.joypad_bits.up;
  prev_joypad_bits.down = gb.direct.joypad_bits.down;
  prev_joypad_bits.left = gb.direct.joypad_bits.left;
  prev_joypad_bits.right = gb.direct.joypad_bits.right;
  prev_joypad_bits.a = gb.direct.joypad_bits.a;
  prev_joypad_bits.b = gb.direct.joypad_bits.b;
  prev_joypad_bits.select = gb.direct.joypad_bits.select;
  prev_joypad_bits.start = gb.direct.joypad_bits.start;
  gb.direct.joypad_bits.up = gpio_get(GPIO_UP);
  gb.direct.joypad_bits.down = gpio_get(GPIO_DOWN);
  gb.direct.joypad_bits.left = gpio_get(GPIO_LEFT);
  gb.direct.joypad_bits.right = gpio_get(GPIO_RIGHT);
  gb.direct.joypad_bits.a = gpio_get(GPIO_A);
  gb.direct.joypad_bits.b = gpio_get(GPIO_B);
  gb.direct.joypad_bits.select = gpio_get(GPIO_SELECT);
  gb.direct.joypad_bits.start = gpio_get(GPIO_START);

  /* hotkeys (select + * combo)*/
  if (!gb.direct.joypad_bits.select) {
#if ENABLE_SOUND
    if (!gb.direct.joypad_bits.up && prev_joypad_bits.up) {
      /* select + up: increase sound volume */
      i2s_increase_volume(&i2s_config);
    }
    if (!gb.direct.joypad_bits.down && prev_joypad_bits.down) {
      /* select + down: decrease sound volume */
      i2s_decrease_volume(&i2s_config);
    }
#endif
    if (!gb.direct.joypad_bits.right && prev_joypad_bits.right) {
      /* select + right: select the next manual color palette */
      nextPalette();
    }
    if (!gb.direct.joypad_bits.left && prev_joypad_bits.left) {
      /* select + left: select the previous manual color palette */
      prevPalette();
    }
    if (!gb.direct.joypad_bits.start && prev_joypad_bits.start) {
      /* select + start: save ram and resets to the game selection menu */
#if ENABLE_SDCARD
      write_cart_ram_file(&gb);
#endif
      stop();
    }
    if (!gb.direct.joypad_bits.a && prev_joypad_bits.a) {
      /* select + A: enable/disable frame-skip => fast-forward */
      gb.direct.frame_skip = !gb.direct.frame_skip;
      Serial.printf("I gb.direct.frame_skip = %d\n", gb.direct.frame_skip);
    }
  }
}

void loop() {
  static uint_fast32_t frames = 0;

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
  handleInput(frames);
}
