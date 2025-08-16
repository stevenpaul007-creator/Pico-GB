#include <TFT_eSPI.h>

#include "common.h"

#define USE_FRAMEBUFFER

static TFT_eSPI tft = TFT_eSPI();
#ifdef USE_FRAMEBUFFER
static TFT_eSprite framebuffer = TFT_eSprite(&tft);
#else
static TFT_eSPI& framebuffer = tft;
#endif

static uint8_t scaledLineOffsetTable[LCD_HEIGHT]; // scaled to 240 lines

/* Pixel data is stored in here. */
static uint8_t pixels_buffer[LCD_WIDTH];

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

#ifdef USE_FRAMEBUFFER
void lcd_pushColors(size_t offset, const uint16_t* pixels, uint_fast16_t count) {
  uint16_t* image = (uint16_t*) framebuffer.getPointer();
  memcpy(&image[offset], pixels, count * sizeof(uint16_t));
}
#else
void lcd_pushColors(uint16_t colOffset, uint16_t lineOffset, const uint16_t* pixels, uint_fast16_t count) {
  framebuffer.setAddrWindow(colOffset, lineOffset, count, 1);
  tft.pushColors((uint16_t*) pixels, count, true);
}
#endif

void lcd_write_pixels_normal(const uint16_t* pixels, uint8_t line, uint_fast16_t count) {
  const uint16_t colOffset = (DISPLAY_WIDTH - count) / 2;
  const uint16_t screenLineOffset = (DISPLAY_HEIGHT - LCD_HEIGHT) / 2;
  const uint16_t lineOffset = screenLineOffset + line;
  lcd_pushColors(lineOffset * DISPLAY_WIDTH + colOffset, pixels, count);
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
#ifdef USE_FRAMEBUFFER
  size_t offset = lineOffset * DISPLAY_WIDTH;
  lcd_pushColors(offset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushColors(offset + DISPLAY_WIDTH, doubledPixels, stretchedWidth);
  }
#else
  lcd_pushColors(0, lineOffset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushColors(0, lineOffset, doubledPixels, stretchedWidth);
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
#ifdef USE_FRAMEBUFFER
  size_t offset = lineOffset * DISPLAY_WIDTH + colOffset;
  lcd_pushColors(offset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushColors(offset + DISPLAY_WIDTH, doubledPixels, stretchedWidth);
  }
#else
  lcd_pushColors(colOffset, lineOffset, doubledPixels, stretchedWidth);
  if (lineRepeated) {
    lcd_pushColors(colOffset, lineOffset, doubledPixels, stretchedWidth);
  }
#endif
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
#ifdef USE_FRAMEBUFFER
  framebuffer.fillSprite(color);
#else
  tft.fillScreen(color);
#endif
}

void lcd_text(char* s, uint8_t x, uint8_t y, uint16_t color, uint16_t bgcolor) {
  framebuffer.setTextColor(TFT_WHITE, TFT_BLACK); // TODO
  framebuffer.drawString(s, x, y);
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
  lcd_fill(TFT_BLACK);

#ifdef USE_FRAMEBUFFER
  framebuffer.setColorDepth(16);
  framebuffer.createSprite(tft.width(), tft.height());
#endif

  calcExtraLineTable();

  // Sleep used for debugging LCD window.
  // sleep_ms(1000);

  while (true) {
    core1DispatchLoop();
  }
}
