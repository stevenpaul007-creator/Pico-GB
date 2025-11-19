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

// C Headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// RP2040 Headers
#include <hardware/vreg.h>

// Project headers
#include "hedley.h"

#include "common.h"
#include "input.h"

#if ENABLE_SDCARD
#include "card_loader.h"
#include "SdFat.h"
#ifdef ENABLE_USB_STORAGE_DEVICE
#include "msc.h"
#endif
#endif

#if ENABLE_SOUND
#include "i2s-audio.h"
#include "minigb_apu.h"

#include "ingamemenu.h"

/**
 * Global variables for audio task
 * stream contains N=AUDIO_SAMPLES samples
 * each sample is 32 bits
 * 16 bits for the left channel + 16 bits for the right channel in stereo interleaved format)
 * This is intended to be played at AUDIO_SAMPLE_RATE Hz
 */
uint16_t* stream;

i2s_config_t i2s_config;
#endif

uint_fast32_t frames = 0;

void startEmulator() {
#if ENABLE_LCD
#if ENABLE_SDCARD
  Serial.println("Starting ROM file selector ...");
  rom_file_selector();
#endif
#endif

  Serial.println("Init GB context ...");  
  initGbContext();

  /* Automatically assign a colour palette to the game */
  char rom_title[16];
  auto_assign_palette(palette, gb_colour_hash(&gb), gb_get_rom_name(&gb, rom_title));

#if ENABLE_LCD
  gb_init_lcd(&gb, &lcd_draw_line);

  /* Start Core1, which processes requests to the LCD. */
  Serial.println("Starting Core1 ...");
  multicore_launch_core1(core1_init);
#endif

#if ENABLE_SOUND
  // Initialize audio emulation
  Serial.println("Starting audio ...");
  audio_init();
#endif

#if ENABLE_SDCARD
  Serial.println("Load save file ...");
  read_cart_ram_file(&gb);
#endif

  Serial.print("\n> ");
}

void reset(uint32_t sleepMs) {
  Serial.println("\nEmulation Ended");

  sleep_ms(sleepMs);

  /* stop lcd task running on core 1 */
  multicore_reset_core1();
  watchdog_reboot(0, 0, 0);
}

void halt() {
  while (true) {}
}

void overclock() {
  const unsigned vco = 1596 * 1000 * 1000; /* 266MHz */
  const unsigned div1 = 6, div2 = 1;

  vreg_set_voltage(VREG_VOLTAGE_1_15);
  sleep_ms(2);
  set_sys_clock_pll(vco, div1, div2);
  sleep_ms(2);
}

void initSound() {
#if ENABLE_SOUND
  Serial.println("Initialize Sound ...");

  // Allocate memory for the stream buffer
  stream = (uint16_t*) malloc(AUDIO_BUFFER_SIZE_BYTES);
  assert(stream != NULL);
  memset(stream, 0, AUDIO_BUFFER_SIZE_BYTES); // Zero out the stream buffer

  // Initialize I2S sound driver
  i2s_config = i2s_get_default_config();
  i2s_config.sample_freq = AUDIO_SAMPLE_RATE;
  i2s_config.dma_trans_count = AUDIO_SAMPLES;
  i2s_config.data_pin = I2S_DIN_PIN,
	i2s_config.clock_pin_base = I2S_BCLK_LRC_PIN_BASE;
  // 尝试使用PIO1，如果失败则使用PIO2
  i2s_config.pio = pio1;  // 使用PIO1专门处理I2S，避免与TFT_eSPI的PIO0冲突
  Serial.printf("Using PIO%d for I2S\n", (i2s_config.pio == pio0) ? 0 : 
                                          (i2s_config.pio == pio1) ? 1 : 2);
  i2s_volume(&i2s_config, 5);
  i2s_init(&i2s_config);

  Serial.println("Sound initialized");
#endif
}

void checkUpdate(){
  if(readJoypad(PIN_SELECT) == 0){
    rp2040.rebootToBootloader();
  }
}

void setup() {
#if ENABLE_LCD
  lcd_init(false);
#endif

  //overclock();

  // Initialise USB serial connection for debugging.
  Serial.begin(115200);
  //while (!Serial) ;
  //delay(2000);

#if ENABLE_SDCARD
  init_sdcard();
#if ENABLE_USB_STORAGE_DEVICE
  initUsbStorageDevice();
#endif
#endif
  
  initJoypad();

  //check select button press to dfu mode
  checkUpdate();

  initSound();

  startEmulator();
}

void loop() {
  gb.gb_frame = 0;

  do {
    //__gb_step_cpu(&gb);
    gb_run_frame(&gb);
    tight_loop_contents();
  } while (HEDLEY_LIKELY(gb.gb_frame == 0));

  frames++;
#if ENABLE_SOUND
  if (i2s_config.volume != 16) {
    audio_callback(NULL, (int16_t*) stream, AUDIO_BUFFER_SIZE_BYTES);
    i2s_dma_write(&i2s_config, (int16_t*) stream);
  }
#endif

  handleJoypad();
  handleSerial();
}

void error(String message) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED);
  tft.drawString(message, 0, ERROR_TEXT_OFFSET, FONT_ID);
  Serial.printf("E %s\n", message.c_str());
  Serial.flush();
  reset(5000);
}
