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

 #include <TFT_eSPI.h>

#include "common.h"

TFT_eSPI tft = TFT_eSPI();
#if ENABLE_LCD_FRAMEBUFFER
static TFT_eSprite framebuffer = TFT_eSprite(&tft);
#else
static TFT_eSPI& framebuffer = tft;
#endif

static uint8_t scaledLineOffsetTable[LCD_HEIGHT]; // scaled to 240 lines

/* Pixel data is stored in here. */
static uint8_t pixels_buffer[LCD_WIDTH];

volatile ScalingMode scalingMode = ScalingMode::STRETCH_KEEP_ASPECT; 

static int lcd_line_busy = 0;

#define IS_REPEATED(pos) ((pos % 2) || (pos % 6 == 0))

static void calcExtraLineTable() {
  uint8_t offset = 0;
  for (uint8_t line = 0; line < LCD_HEIGHT; ++line) {
    scaledLineOffsetTable[line] = offset;
    offset += 1 + IS_REPEATED(line);
  }
}

void lcd_init(bool isCore1) {
  tft.init();
#if ENABLE_LCD_DMA
  // do not enable DMA on core0 as it fails on core1 if it is already enabled
  if (isCore1 && !tft.initDMA(/*ctrl_cs not supported in RP2040 implementation*/)) {
    error("Failed to initialize TFT DMA");
  }
#endif
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
}

void lcd_draw_line(struct gb_s* gb, const uint8_t pixels[LCD_WIDTH], const uint_fast8_t line) {
  union core_cmd cmd;

  /* Wait until previous line is sent. */
  while (__atomic_load_n(&lcd_line_busy, __ATOMIC_SEQ_CST))
    tight_loop_contents();

  memcpy(pixels_buffer, pixels, LCD_WIDTH);

  /* Populate command. */
  cmd.cmd = CORE_CMD_LCD_LINE;
  cmd.data = line;

  __atomic_store_n(&lcd_line_busy, 1, __ATOMIC_SEQ_CST);
  multicore_fifo_push_blocking(cmd.full);
}

#if ENABLE_LCD_FRAMEBUFFER
void lcd_pushColors(size_t offset, const uint16_t* pixels, uint_fast16_t count) {
  uint16_t* image = (uint16_t*) framebuffer.getPointer();
  memcpy(&image[offset], pixels, count * sizeof(uint16_t));
}
#else
void lcd_pushColors(uint16_t colOffset, uint16_t lineOffset, const uint16_t* pixels, uint_fast16_t count) {
  framebuffer.setAddrWindow(colOffset, lineOffset, count, 1);
#if ENABLE_LCD_DMA
  // DMA mode does not have a measurable effect without a framebuffer
  static uint16_t dmaBuffer[DISPLAY_WIDTH];
  tft.dmaWait();
  memcpy(dmaBuffer, pixels, count * sizeof(uint16_t));
  tft.setSwapBytes(true);
  tft.startWrite(); // manual start required as DMA transfer is asynchronous
  tft.pushPixelsDMA((uint16_t*) dmaBuffer, count);
  //tft.endWrite(); // do not call endWrite(), as it will wait for the DMA transfer to finish, which results in no performance gain
#else
  tft.pushColors((uint16_t*) pixels, count, true);
#endif
}
#endif

void lcd_write_pixels_normal(const uint16_t* pixels, uint8_t line, uint_fast16_t count) {
  const uint16_t colOffset = (DISPLAY_WIDTH - count) / 2;
  const uint16_t screenLineOffset = (DISPLAY_HEIGHT - LCD_HEIGHT) / 2;
  const uint16_t lineOffset = screenLineOffset + line;
#if ENABLE_LCD_FRAMEBUFFER
  lcd_pushColors(lineOffset * DISPLAY_WIDTH + colOffset, pixels, count);
#else
  lcd_pushColors(colOffset, lineOffset, pixels, count);
#endif
}

void lcd_write_pixels_stretched(const uint16_t* pixels, uint8_t line, uint_fast16_t count) {
  static uint16_t doubledPixels[DISPLAY_WIDTH];
  uint16_t pos = 0;
  for (int col = 0; col < count; ++col) {
    doubledPixels[pos++] = pixels[col];
    doubledPixels[pos++] = pixels[col];
  }
  const uint16_t stretchedWidth = pos;

  const uint8_t lineRepeated = IS_REPEATED(line);
  const uint16_t lineOffset = scaledLineOffsetTable[line];
#if ENABLE_LCD_FRAMEBUFFER
  size_t offset = lineOffset * DISPLAY_WIDTH;
  lcd_pushColors(offset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushColors(offset + DISPLAY_WIDTH, doubledPixels, stretchedWidth);
  }
#else
  lcd_pushColors(0, lineOffset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushColors(0, lineOffset + 1, doubledPixels, stretchedWidth);
  }
#endif
}

void lcd_write_pixels_stretched_keep_aspect(const uint16_t* pixels, uint8_t line, uint_fast16_t count) {
  static uint16_t doubledPixels[DISPLAY_WIDTH];
  uint16_t pos = 0;
  for (int col = 0; col < count; ++col) {
    doubledPixels[pos++] = pixels[col];
    if (IS_REPEATED(col)) {
      doubledPixels[pos++] = pixels[col];
    }
  }
  const uint16_t stretchedWidth = pos;

  const uint16_t colOffset = (DISPLAY_WIDTH - stretchedWidth) / 2;
 
  uint8_t lineRepeated = IS_REPEATED(line);
  const uint16_t lineOffset = scaledLineOffsetTable[line];
#if ENABLE_LCD_FRAMEBUFFER
  size_t offset = lineOffset * DISPLAY_WIDTH + colOffset;
  lcd_pushColors(offset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushColors(offset + DISPLAY_WIDTH, doubledPixels, stretchedWidth);
  }
#else
  lcd_pushColors(colOffset, lineOffset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushColors(colOffset, lineOffset + 1, doubledPixels, stretchedWidth);
  }
#endif
}

// Writes pixels to screen or framebuffer
void lcd_write_pixels(const uint16_t* pixels, uint8_t line, uint_fast16_t nmemb) {
  switch (scalingMode)
  {
  case ScalingMode::STRETCH:
    lcd_write_pixels_stretched(pixels, line, nmemb);
    break;
  case ScalingMode::STRETCH_KEEP_ASPECT:
    lcd_write_pixels_stretched_keep_aspect(pixels, line, nmemb);
    break;
  case ScalingMode::NORMAL:
  default:
    lcd_write_pixels_normal(pixels, line, nmemb);
    break;
  }
}

#if ENABLE_LCD_FRAMEBUFFER
// Writes framebuffer to screen
void lcd_write_framebuffer_to_screen() {
    tft.setSwapBytes(true);
#ifdef ENABLE_LCD_DMA
    tft.startWrite(); // manual start required as DMA transfer is asynchronous
    tft.pushImageDMA(0, 0, framebuffer.width(), framebuffer.height(), (uint16_t *) framebuffer.getPointer());
    //tft.endWrite(); // do not call endWrite(), as it will wait for the DMA transfer to finish, which results in no performance gain
#else
    tft.pushImage(0, 0, framebuffer.width(), framebuffer.height(), (uint16_t *) framebuffer.getPointer());
#endif
}
#endif

void lcd_fill(uint16_t color) {
#if ENABLE_LCD_FRAMEBUFFER
  framebuffer.fillSprite(color);
#else
  tft.fillScreen(color);
#endif
}

void core1_lcd_draw_line(const uint_fast8_t line) {
  static uint16_t fb[LCD_WIDTH];

  if (gb.cgb.cgbMode) {
    for (unsigned int x = 0; x < LCD_WIDTH; x++) {
      fb[x] = gb.cgb.fixPalette[pixels_buffer[x]];
    }
  } else {
    for (unsigned int x = 0; x < LCD_WIDTH; x++) {
      fb[x] = palette[(pixels_buffer[x] & LCD_PALETTE_ALL) >> 4]
                    [pixels_buffer[x] & 3];
    }
  }

  lcd_write_pixels(fb, line, LCD_WIDTH);
  __atomic_store_n(&lcd_line_busy, 0, __ATOMIC_SEQ_CST);

#if ENABLE_LCD_FRAMEBUFFER
  if (line == LCD_HEIGHT - 1) {
    lcd_write_framebuffer_to_screen();
  }
#endif
}

void core1DispatchLoop() {
  union core_cmd cmd;

  /* Handle commands coming from core0. */
  cmd.full = multicore_fifo_pop_blocking();
  switch (cmd.cmd) {
  case CORE_CMD_LCD_LINE:
    core1_lcd_draw_line(cmd.data);
    break;

  case CORE_CMD_IDLE_SET:
    lcd_fill(TFT_BLACK);
    break;

  case CORE_CMD_NOP:
  default:
    break;
  }
}

void core1_init() {
  /* Initialise and control LCD on core 1. */
  lcd_init(true);

#if ENABLE_LCD_FRAMEBUFFER
  framebuffer.setColorDepth(16);
  framebuffer.createSprite(tft.width(), tft.height());
#endif

  /* Clear LCD screen. */
  lcd_fill(TFT_BLACK);

  calcExtraLineTable();

  while (true) {
    core1DispatchLoop();
  }
}
