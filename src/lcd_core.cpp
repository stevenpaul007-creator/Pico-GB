#include <TFT_eSPI.h>

#define PEANUT_GB_HEADER_ONLY
#include "peanut_gb_options.h"
#include "peanut_gb.h"

// Note: must be included before core
#include "gbcolors.h"
#include "core.h"

static TFT_eSPI tft = TFT_eSPI();

static uint8_t scaledLineOffsetTable[LCD_HEIGHT]; // scaled to 240 lines

/* Pixel data is stored in here. */
static uint8_t pixels_buffer[LCD_WIDTH];

extern struct gb_s gb;
extern palette_t palette; // Colour palette

volatile ScalingMode scalingMode = ScalingMode::NORMAL; 

static int lcd_line_busy = 0;

#define IS_REPEATED(pos) ((pos % 2) || (pos % 6 == 0))

static void calcExtraLineTable() {
  uint8_t offset = 0;
  for (uint8_t line = 0; line < LCD_HEIGHT; ++line) {
    scaledLineOffsetTable[line] = offset;
    offset += 1 + IS_REPEATED(line);
  }
}

void lcd_init(void) {
  tft.init();
  // tft.initDMA();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
}

void lcd_draw_line(struct gb_s* gb, const uint8_t pixels[LCD_WIDTH],
    const uint_fast8_t line) {
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

void lcd_write_pixels_normal(const uint16_t* pixels, uint8_t line, uint_fast16_t nmemb) {
  const uint16_t colOffset = (tft.width() - nmemb) / 2;
  const uint16_t lineOffset = (tft.height() - LCD_HEIGHT) / 2;
  tft.setAddrWindow(colOffset, lineOffset + line, nmemb, 1);
  tft.pushColors((uint16_t*) pixels, nmemb, true);
}

void lcd_write_pixels_stretched(const uint16_t* pixels, uint8_t line, uint_fast16_t nmemb) {
  static uint16_t doubledPixels[320];
  uint16_t pos = 0;
  for (int col = 0; col < nmemb; ++col) {
    doubledPixels[pos++] = pixels[col];
    doubledPixels[pos++] = pixels[col];
  }
  const uint16_t stretchedWidth = pos;

  uint8_t repeatedLines = IS_REPEATED(line);
  tft.setAddrWindow(0, scaledLineOffsetTable[line], stretchedWidth, 1);
  tft.pushColors((uint16_t*)doubledPixels, stretchedWidth, true);
  if (repeatedLines) {
    tft.setAddrWindow(0, scaledLineOffsetTable[line] + 1, stretchedWidth, 1);
    tft.pushColors((uint16_t*)doubledPixels, stretchedWidth, true);
  }
}

void lcd_write_pixels_stretched_keep_aspect(const uint16_t* pixels, uint8_t line, uint_fast16_t nmemb) {
  static uint16_t doubledPixels[320];
  uint16_t pos = 0;
  for (int col = 0; col < nmemb; ++col) {
    doubledPixels[pos++] = pixels[col];
    if (IS_REPEATED(col)) {
      doubledPixels[pos++] = pixels[col];
    }
  }
  const uint16_t stretchedWidth = pos;

  const uint16_t colOffset = (tft.width() - stretchedWidth) / 2;
 
  uint8_t repeatedLines = IS_REPEATED(line);
  tft.setAddrWindow(colOffset, scaledLineOffsetTable[line], stretchedWidth, 1);
  tft.pushColors((uint16_t*)doubledPixels, stretchedWidth, true);
  if (repeatedLines) {
    tft.setAddrWindow(colOffset, scaledLineOffsetTable[line] + 1, stretchedWidth, 1);
    tft.pushColors((uint16_t*)doubledPixels, stretchedWidth, true);
  }
}

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

void lcd_fill(uint16_t color) {
  tft.fillScreen(color);
}

void lcd_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint16_t color) {
  tft.fillRect(0, 0, tft.width(), tft.height(), TFT_BLACK);
}

void lcd_text(char* s, uint8_t x, uint8_t y, uint16_t color, uint16_t bgcolor) {
  tft.setTextColor(TFT_WHITE, TFT_BLACK); // TODO
  tft.drawString(s, x, y);
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
  lcd_init();

  /* Clear LCD screen. */
  lcd_fill(0x0000);

  /* Set LCD window to DMG size. */
  lcd_fill_rect(31, 16, LCD_WIDTH, LCD_HEIGHT, 0x0000);

  calcExtraLineTable();

  // Sleep used for debugging LCD window.
  // sleep_ms(1000);

  while (true) {
    core1DispatchLoop();
  }
}
