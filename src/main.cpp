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
#include "peanut_gb_options.h"
#include "peanut_gb.h"

/* C Headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RP2040 Headers */
#include <hardware/vreg.h>

#include <PCF8574.h>

/* Project headers */
#include "hedley.h"
#include "minigb_apu.h"

// #include "sdcard.h"
#include "core.h"
#include "game_bin.h"
#include "i2s.h"

#define GBCOLOR_HEADER_ONLY
#include "gbcolors.h"

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

static PCF8574 pcf8574(0x20, 16, 17);

struct gb_s gb;
palette_t palette; // Colour palette

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

void reset();

void startEmulator() {
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

#ifdef USE_PAD_GPIO
void initJoypad() {
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
}
#else
void initJoypad() {
  for (int pin = 0; pin < 8; ++pin) {
    pcf8574.pinMode(pin, INPUT_PULLUP);
  }

  pcf8574.setLatency(5);
  
  if (pcf8574.begin()){
		Serial.println("PCF8574 initialized");
	}else{
		Serial.println("PCF8574 initialization failed");
    while (true) ;
	}
}
#endif

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

void handleSerial(uint_fast32_t& frames) {
  static uint64_t start_time = time_us_64();

  /* Serial monitor commands */
  int input = Serial.read();
  if (input <= 0) {
    return;
  }

  switch (input) {
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
    reset();

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

#ifdef USE_PAD_GPIO
  gb.direct.joypad_bits.up = gpio_get(PIN_UP);
  gb.direct.joypad_bits.down = gpio_get(PIN_DOWN);
  gb.direct.joypad_bits.left = gpio_get(PIN_LEFT);
  gb.direct.joypad_bits.right = gpio_get(PIN_RIGHT);
  gb.direct.joypad_bits.a = gpio_get(PIN_A);
  gb.direct.joypad_bits.b = gpio_get(PIN_B);
  gb.direct.joypad_bits.select = gpio_get(PIN_SELECT);
  gb.direct.joypad_bits.start = gpio_get(PIN_START);
#else
  #if 0
  Serial.printf("pins:\n");
  for (int i = 0; i < 8; ++i) {
    int in = pcf8574.digitalRead(i);
    Serial.printf(" %d", in);
  }
  Serial.printf("\n");
  #endif

  gb.direct.joypad_bits.up = pcf8574.digitalRead(PIN_UP);
  gb.direct.joypad_bits.down = pcf8574.digitalRead(PIN_DOWN);
  gb.direct.joypad_bits.left = pcf8574.digitalRead(PIN_LEFT);
  gb.direct.joypad_bits.right = pcf8574.digitalRead(PIN_RIGHT);
  gb.direct.joypad_bits.a = pcf8574.digitalRead(PIN_A);
  gb.direct.joypad_bits.b = pcf8574.digitalRead(PIN_B);
  gb.direct.joypad_bits.select = pcf8574.digitalRead(PIN_SELECT);
  gb.direct.joypad_bits.start = pcf8574.digitalRead(PIN_START);
#endif

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
      reset();
    }
    if (!gb.direct.joypad_bits.a && prev_joypad_bits.a) {
      /* select + A: enable/disable frame-skip => fast-forward */
      gb.direct.frame_skip = !gb.direct.frame_skip;
      Serial.printf("I gb.direct.frame_skip = %d\n", gb.direct.frame_skip);
    }
    if (!gb.direct.joypad_bits.b && prev_joypad_bits.b) {
      /* select + B: change scaling mode */
      scalingMode = (ScalingMode)(((int) scalingMode + 1) % ((int) ScalingMode::COUNT));
      union core_cmd cmd;
      cmd.cmd = CORE_CMD_IDLE_SET;
      multicore_fifo_push_blocking(cmd.full);
      Serial.printf("I Scaling mode: = %d\n", scalingMode);
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
  handleSerial(frames);
}
