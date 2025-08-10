#include <TFT_eSPI.h>

#define PEANUT_GB_HEADER_ONLY
#include "peanut_gb.h"

// Note: must be included before core
#include "gbcolors.h"
#include "core.h"

static TFT_eSPI tft = TFT_eSPI();

static uint8_t scaledLineOffsetTable[LCD_HEIGHT]; // scaled to 240 lines

/* Pixel data is stored in here. */
static uint8_t pixels_buffer[LCD_WIDTH];

palette_t palette; // Colour palette

static int lcd_line_busy = 0;

#define IS_LINE_REPEATED(line) ((line % 2) || (line % 6 == 0))
// #define IS_LINE_REPEATED(line) 0

static void calcExtraLineTable() {
  uint8_t offset = 0;
  for (uint8_t line = 0; line < LCD_HEIGHT; ++line) {
    scaledLineOffsetTable[line] = offset;
    offset += 1 + IS_LINE_REPEATED(line);
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

void lcd_write_pixels(const uint16_t* pixels, uint8_t line, uint_fast16_t nmemb) {
  static uint16_t doubledPixels[320];
  uint16_t pos = 0;
  for (int i = 0; i < nmemb; ++i) {
    doubledPixels[pos++] = pixels[i];
    doubledPixels[pos++] = pixels[i];
  }

  uint8_t repeatedLines = IS_LINE_REPEATED(line);
  // tft.setAddrWindow(0, scaledLineOffsetTable[line], nmemb * 2, 1 + repeatedLines);
  tft.setAddrWindow(0, scaledLineOffsetTable[line], nmemb * 2, 1);
  tft.pushColors((uint16_t*)doubledPixels, nmemb * 2, true);
  if (repeatedLines) {
    tft.setAddrWindow(0, scaledLineOffsetTable[line] + 1, nmemb * 2, 1);
    tft.pushColors((uint16_t*)doubledPixels, nmemb * 2, true);
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

void lcd_display_control(bool invert, int /*ili9225_color_mode_e*/ colour_mode) {
  // TODO
}

void core1_lcd_draw_line(const uint_fast8_t line) {
  static uint16_t fb[LCD_WIDTH];

  for (unsigned int x = 0; x < LCD_WIDTH; x++) {
    fb[x] = palette[(pixels_buffer[x] & LCD_PALETTE_ALL) >> 4]
                   [pixels_buffer[x] & 3];
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
    lcd_display_control(true, cmd.data);
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
