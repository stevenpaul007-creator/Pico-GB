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
#if ENABLE_FRAMEBUFFER_FLIP_X_Y
#define FRAMEBUFFER_WIDTH DISPLAY_HEIGHT
#define FRAMEBUFFER_HEIGHT DISPLAY_WIDTH
#else
#define FRAMEBUFFER_WIDTH DISPLAY_WIDTH
#define FRAMEBUFFER_HEIGHT DISPLAY_HEIGHT
#endif
#define FRAMEBUFFER_PIXELS (FRAMEBUFFER_WIDTH * FRAMEBUFFER_HEIGHT)
#if ENABLE_LCD_DMA && ENABLE_DOUBLE_BUFFERING
#define BUFFER_COUNT 2
#else // !ENABLE_LCD_DMA
#define BUFFER_COUNT 1 // no use in double buffering without DMA
#endif
static uint16_t framebuffers[BUFFER_COUNT][FRAMEBUFFER_PIXELS];
static int8_t activeFramebufferId = 0;
#else // !ENABLE_LCD_FRAMEBUFFER
// Note DMA mode does not have a measurable effect without a framebuffer
static uint16_t linebuffer[DISPLAY_WIDTH];
#endif

static uint8_t scaledLineOffsetTable[LCD_HEIGHT]; // scaled to 240 lines

/* Pixel data is stored in here. */
static uint8_t pixels_buffer[LCD_WIDTH];

volatile ScalingMode scalingMode = ScalingMode::STRETCH; 

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
  spi_init(spi0, SPI_FREQUENCY);

  bool rotate = true;
#if ENABLE_FRAMEBUFFER_FLIP_X_Y
  // the rotation only flips x and y in GRAM but does not change the LCD screen refresh direction.
  // If the display is a native portrait (and not landscape) lcd, rotating the lcd to landscape results in ugly
  // diagonal update lines. Keeping it in portrait mode and Flipping x and y in the framebuffer results in nicer
  // horizontal update lines. Even better would be VSync or TE line handling (which is not present in most cheap displays)
  rotate = !isCore1; // rotate in start menu but not in-game 
#endif
  tft.setRotation(rotate ? 3 : 2);
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
void lcd_pushLine(uint16_t screenColOffset, uint16_t screenLineOffset, uint16_t line, const uint16_t* pixels, uint_fast16_t width) {
  uint16_t* framebuffer = framebuffers[activeFramebufferId];
#if ENABLE_FRAMEBUFFER_FLIP_X_Y
  uint_fast16_t pos = (screenColOffset * DISPLAY_HEIGHT) + DISPLAY_HEIGHT - (screenLineOffset + line) - 1;
  for (uint_fast16_t i = 0; i < width; ++i) {
    framebuffer[pos] = pixels[i];
    pos += DISPLAY_HEIGHT;
  }
#else
  uint32_t offset = screenColOffset + (uint32_t)(screenLineOffset + line) * DISPLAY_WIDTH;
  memcpy(&framebuffer[offset], pixels, width * sizeof(uint16_t));
#endif
}
#else // !ENABLE_LCD_FRAMEBUFFER
void lcd_pushLine(uint16_t screenColOffset, uint16_t screenLineOffset, uint16_t line, const uint16_t* pixels, uint_fast16_t width) {
  tft.setAddrWindow(screenColOffset, screenLineOffset + line, width, 1);
#if ENABLE_LCD_DMA
  // 只在必要时等待DMA完成，避免不必要的阻塞
  if (tft.dmaBusy()) {
    tft.dmaWait();
  }
  memcpy(linebuffer, pixels, width * sizeof(uint16_t));
  tft.setSwapBytes(true);
  tft.startWrite(); // manual start required as DMA transfer is asynchronous
  tft.pushPixelsDMA((uint16_t*) linebuffer, width);
  //tft.endWrite(); // do not call endWrite(), as it will wait for the DMA transfer to finish, which results in no performance gain
#else
  tft.pushColors((uint16_t*) pixels, width, true);
#endif
}
#endif

void lcd_write_pixels_normal(const uint16_t* pixels, uint8_t line, uint_fast16_t count) {
  const uint16_t colOffset = (DISPLAY_WIDTH - LCD_WIDTH) / 2;
  const uint16_t screenLineOffset = (DISPLAY_HEIGHT - LCD_HEIGHT) / 2;
  lcd_pushLine(colOffset, screenLineOffset, line, pixels, count);
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
  lcd_pushLine(0, 0, lineOffset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushLine(0, 0, lineOffset + 1, doubledPixels, stretchedWidth);
  }
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
 
  const uint8_t lineRepeated = IS_REPEATED(line);
  const uint16_t lineOffset = scaledLineOffsetTable[line];
  lcd_pushLine(colOffset, 0, lineOffset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushLine(colOffset, 0, lineOffset + 1, doubledPixels, stretchedWidth);
  }
}

// Writes pixels to screen or framebuffer
void lcd_write_pixels(const uint16_t* pixels, uint8_t line, uint_fast16_t count) {
  switch (scalingMode)
  {
  case ScalingMode::STRETCH:
    lcd_write_pixels_stretched(pixels, line, count);
    break;
  case ScalingMode::STRETCH_KEEP_ASPECT:
    lcd_write_pixels_stretched_keep_aspect(pixels, line, count);
    break;
  case ScalingMode::NORMAL:
  default:
    lcd_write_pixels_normal(pixels, line, count);
    break;
  }
}


#if ENABLE_LCD_FRAMEBUFFER

void lcd_swap_buffers() {
#if BUFFER_COUNT == 2
  activeFramebufferId = (activeFramebufferId == 0) ? 1 : 0;
#endif
}

// Writes framebuffer to screen
void lcd_write_framebuffer_to_screen() {
  
  uint16_t* framebuffer = framebuffers[activeFramebufferId];
  tft.setSwapBytes(true);
#if ENABLE_LCD_DMA
  //优化双缓冲策略：检查DMA状态，避免阻塞
  if (tft.dmaBusy()) {
    // DMA忙碌时，跳过这一帧，避免阻塞
    return;
  }
  lcd_swap_buffers();
  tft.startWrite(); // manual start required as DMA transfer is asynchronous
  tft.pushImageDMA(0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, framebuffer);
  // tft.endWrite(); // do not call endWrite(), as it will wait for the DMA transfer to finish, which results in no performance gain
#else
  tft.pushImage(0, 0, FRAMEBUFFER_WIDTH, FRAMEBUFFER_HEIGHT, framebuffer);
#endif
}

#endif


void lcd_clear() {
#if ENABLE_LCD_FRAMEBUFFER
  memset(framebuffers[0], 0, FRAMEBUFFER_PIXELS * sizeof(uint16_t));
#if BUFFER_COUNT == 2
  memset(framebuffers[1], 0, FRAMEBUFFER_PIXELS * sizeof(uint16_t));
#endif
#else
  tft.fillScreen(TFT_BLACK);
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

  // Handle commands coming from core0
  cmd.full = multicore_fifo_pop_blocking();
  switch (cmd.cmd) {
  case CORE_CMD_LCD_LINE:
    core1_lcd_draw_line(cmd.data);
    break;

  case CORE_CMD_IDLE_SET:
    lcd_clear();
    break;

  case CORE_CMD_NOP:
  default:
    break;
  }
}

void core1_init() {
  // Initialise and control LCD on core 1
  lcd_init(true);

  lcd_clear();

  calcExtraLineTable();

  while (true) {
    core1DispatchLoop();
  }
}
