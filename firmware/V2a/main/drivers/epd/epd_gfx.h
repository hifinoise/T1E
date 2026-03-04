#pragma once
#include <stdint.h>
#include "hal_pins.h"
#include "gfxfont.h"

#define GFX_BLACK 0
#define GFX_WHITE 1

// Lovelo Black at three sizes — the only font you'll ever need
typedef enum {
    FONT_SMALL,     // Lovelo Black 9pt  — corner clock, hints
    FONT_MEDIUM,    // Lovelo Black 14pt — date, labels, settings
    FONT_LARGE,     // Lovelo Black 28pt — time digits, big numbers
} gfx_font_t;

void gfx_clear(uint8_t *fb, uint8_t color);
void gfx_pixel(uint8_t *fb, int16_t x, int16_t y, uint8_t color);
uint8_t gfx_get_pixel(const uint8_t *fb, int16_t x, int16_t y);
void gfx_hline(uint8_t *fb, int16_t x, int16_t y, int16_t w, uint8_t color);
void gfx_vline(uint8_t *fb, int16_t x, int16_t y, int16_t h, uint8_t color);
void gfx_rect(uint8_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
void gfx_fill_rect(uint8_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color);
int16_t gfx_draw_char(uint8_t *fb, int16_t x, int16_t y, char c, gfx_font_t font, uint8_t color);
int16_t gfx_puts(uint8_t *fb, int16_t x, int16_t y, const char *str, gfx_font_t font, uint8_t color);
int16_t gfx_text_width(const char *str, gfx_font_t font);
int16_t gfx_font_height(gfx_font_t font);
void gfx_puts_centered(uint8_t *fb, int16_t y, const char *str, gfx_font_t font, uint8_t color);
