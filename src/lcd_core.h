
#pragma once

#include <TFT_eSPI.h>

#include "common.h"

extern TFT_eSPI tft;
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
static uint16_t pixels_buffer[DISPLAY_WIDTH];
// origional lcd width. for gb is 166
extern uint_fast16_t max_lcd_width;
// origional lcd height. for gb is 144
extern uint_fast16_t max_lcd_height;

extern volatile ScalingMode scalingMode; 

static int lcd_line_busy = 0;

#define IS_REPEATED(pos) ((pos % 2) || (pos % 6 == 0))

void lcd_init(bool isCore1);
void lcd_draw_line(struct gb_s* gb, const uint16_t* pixels, const uint_fast8_t line);

void lcd_draw_line_8bits(struct gb_s* gb, const uint8_t* pixels, const uint_fast8_t line);

void core1_init();
void core1DispatchLoop();
void core1_lcd_draw_line(const uint_fast8_t line);
void lcd_clear();