#pragma once

#include <Arduino.h>
#if ENABLE_LCD
#include <TFT_eSPI.h>
#endif
#include <SdFat.h>
#include "gb.h"

/* Joypad Pins. */
#define USE_JOYPAD_I2C_IO_EXPANDER
#ifdef USE_JOYPAD_I2C_IO_EXPANDER
// Use PCF8574 for Joypad. Only required if an LCD with 16-bit parallel bus is used,
// as Pico does not have enough pins for all peripherals. With 8-bit parallel or SPI LCDs this should not be necessary.
#define PCF8574_ADDR 0x20
#define PCF8574_SDA 20
#define PCF8574_SCL 21
// pins below are on the IO expander
#define PIN_UP		0
#define PIN_DOWN	1 
#define PIN_LEFT	2
#define PIN_RIGHT	3
#define PIN_A		5
#define PIN_B		4
#define PIN_SELECT	6
#define PIN_START	7
#else
// Use GPIOs directly on Pico for Joypad
#define PIN_UP		2
#define PIN_DOWN	3
#define PIN_LEFT	4
#define PIN_RIGHT	5
#define PIN_A		6
#define PIN_B		7
#define PIN_SELECT	8
#define PIN_START	9
#endif

#if ENABLE_SOUND
#define I2S_DIN_PIN 26
#define I2S_BCLK_LRC_PIN_BASE 27  // BCLK + LRC (28)
#endif

#if ENABLE_SDCARD
#define SD_CS_PIN 17
#define SD_SCK_PIN 18
#define SD_MOSI_PIN 19
#define SD_MISO_PIN 16

extern uint8_t _FS_start;
extern uint8_t _FS_end;
#define MAX_ROM_SIZE (&_FS_end - &_FS_start)
#endif

// display is rotated, so TFT_WIDTH/HEIGHT cannot be used
#define DISPLAY_WIDTH TFT_HEIGHT
#define DISPLAY_HEIGHT TFT_WIDTH

#define FONT_HEIGHT 8
#define ERROR_TEXT_OFFSET FONT_HEIGHT
#define FONT_ID 1

enum class ScalingMode {
  NORMAL = 0,
  STRETCH,
  STRETCH_KEEP_ASPECT,
  COUNT
};

extern volatile ScalingMode scalingMode; 

extern uint_fast32_t frames;
extern TFT_eSPI tft;

/* Multicore command structure. */
union core_cmd {
  struct
  {
    /* Does nothing. */
#define CORE_CMD_NOP 0
    /* Set line "data" on the LCD. Pixel data is in pixels_buffer. */
#define CORE_CMD_LCD_LINE 1
    /* Control idle mode on the LCD. Limits colours to 2 bits. */
#define CORE_CMD_IDLE_SET 2
    /* Set a specific pixel. For debugging. */
#define CORE_CMD_SET_PIXEL 3
    uint8_t cmd;
    uint8_t unused1;
    uint8_t unused2;
    uint8_t data;
  };
  uint32_t full;
};

void lcd_init(bool isCore1);
void lcd_draw_line(struct gb_s* gb, const uint8_t* pixels, const uint_fast8_t line);

void core1_init();

void reset(uint32_t sleepMs = 0);

void error(String message);
