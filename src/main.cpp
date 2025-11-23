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

#include <Arduino.h>
#include "gb.h"

// C Headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lcd_core.h"

// RP2040 Headers
#include <hardware/vreg.h>



#if ENABLE_SDCARD
#ifdef ENABLE_USB_STORAGE_DEVICE
#include "msc.h"
#endif
#endif

#include "allmenus.h"
#include "allservices.h"
#include "gbinput.h"

GBInput gbInput;

#include "nesinput.h"

uint_fast32_t frames = 0;

void startGBEmulator() {
  gbInput.initJoypad();
  scalingMode = ScalingMode::STRETCH;
  Serial.println("Init GB context ...");  
  initGbContext();

  /* Automatically assign a colour palette to the game */
  char rom_title[16];
  auto_assign_palette(palette, gb_colour_hash(&gb), gb_get_rom_name(&gb, rom_title));

#if ENABLE_LCD
  gb_init_lcd(&gb, &lcd_draw_line_8bits);

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
  srv.cardService.read_cart_ram_file(&gb);
#endif

  Serial.print("\n> ");
}

void startNESEmulator() {
  scalingMode = ScalingMode::NORMAL;
#if ENABLE_LCD
  /* Start Core1, which processes requests to the LCD. */
  Serial.println("Starting Core1 ...");
  multicore_launch_core1(core1_init);
#endif

#if ENABLE_SOUND
  // Initialize audio emulation
  Serial.println("Starting audio ...");
#endif


  Serial.print("\n> ");
  nesMain();
}

void reset(uint32_t sleepMs) {
  Serial.println("\nEmulation Ended");

  sleep_ms(sleepMs);

  rp2040.reboot();
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

void checkUpdate(){
  if(srv.inputService.readJoypad(ButtonID::BTN_SELECT) == 0){
    rp2040.rebootToBootloader();
  }
}

void setup() {
#if ENABLE_LCD
  lcd_init(false);
#endif

  overclock();

  // Initialise USB serial connection for debugging.
  Serial.begin(115200);
  while (!Serial) ;
  // delay(2000);
  Serial.println("I Serial OK.");

#if ENABLE_SDCARD
#if ENABLE_USB_STORAGE_DEVICE
  initUsbStorageDevice();
#endif
#endif
  
  srv.initAll();

  //check select button press to dfu mode
  checkUpdate();

#if ENABLE_LCD
#if ENABLE_SDCARD
  Serial.println("Starting ROM file selector ...");
  srv.cardService.rom_file_selector();
#endif
#endif
  gameType = srv.cardService.getSelectedFileType();
  switch(gameType){
    case GameType_GB:
      startGBEmulator();
      break;
    case GameType_NES:
      startNESEmulator();
      break;
  }
}

void loop() {
  switch (srv.cardService.getSelectedFileType()) {
  case GameType_GB:
    gb.gb_frame = 0;

    do {
      //__gb_step_cpu(&gb);
      gb_run_frame(&gb);
      tight_loop_contents();
    } while (HEDLEY_LIKELY(gb.gb_frame == 0));

    frames++;
#if ENABLE_SOUND
    srv.soundService.handleSoundLoop();
#endif
    break;
  case GameType_NES:
    InfoNES_Main();
    break;
  }
  srv.inputService.handleJoypad();
  srv.inputService.handleSerial();

}

void error(String message) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_RED);
  tft.drawString(message, 0, ERROR_TEXT_OFFSET, FONT_ID);
  Serial.printf("E %s\n", message.c_str());
  Serial.flush();
  reset(5000);
}
