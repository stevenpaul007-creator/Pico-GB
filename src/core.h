#pragma once

#include <Arduino.h>

#define GBCOLOR_HEADER_ONLY
#include "gbcolors.h"

/* Multicore command structure. */
union core_cmd
{
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


extern palette_t palette; // Colour palette

void lcd_draw_line(struct gb_s *gb, const uint8_t *pixels, const uint_fast8_t line);

void core1_init();
