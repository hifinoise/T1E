#include "epd_gfx.h"
#include "gfxfont.h"
#include "LoveloBlack28pt.h"
#include "LoveloBlack14pt.h"
#include "LoveloBlack9pt.h"
#include <string.h>

/* ---- Primitives ---- */

void gfx_clear(uint8_t *fb, uint8_t color) {
    memset(fb, color ? 0xFF : 0x00, EPD_FB_SIZE);
}

void gfx_pixel(uint8_t *fb, int16_t x, int16_t y, uint8_t color) {
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) return;
    uint16_t idx = y * (EPD_WIDTH / 8) + (x / 8);
    uint8_t bit = 0x80 >> (x % 8);
    if (color) fb[idx] |= bit; else fb[idx] &= ~bit;
}

uint8_t gfx_get_pixel(const uint8_t *fb, int16_t x, int16_t y) {
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT) return 0;
    uint16_t idx = y * (EPD_WIDTH / 8) + (x / 8);
    return (fb[idx] & (0x80 >> (x % 8))) ? 1 : 0;
}

void gfx_hline(uint8_t *fb, int16_t x, int16_t y, int16_t w, uint8_t color) {
    for (int16_t i = 0; i < w; i++) gfx_pixel(fb, x + i, y, color);
}

void gfx_vline(uint8_t *fb, int16_t x, int16_t y, int16_t h, uint8_t color) {
    for (int16_t i = 0; i < h; i++) gfx_pixel(fb, x, y + i, color);
}

void gfx_rect(uint8_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    gfx_hline(fb, x, y, w, color);
    gfx_hline(fb, x, y + h - 1, w, color);
    gfx_vline(fb, x, y, h, color);
    gfx_vline(fb, x + w - 1, y, h, color);
}

void gfx_fill_rect(uint8_t *fb, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t color) {
    for (int16_t row = y; row < y + h; row++)
        gfx_hline(fb, x, row, w, color);
}

/* ---- Font Rendering ---- */

static const GFXfont* get_font(gfx_font_t font) {
    switch (font) {
        case FONT_LARGE:  return &Lovelo_Black28pt7b;
        case FONT_MEDIUM: return &Lovelo_Black14pt7b;
        default:          return &Lovelo_Black9pt7b;
    }
}

int16_t gfx_draw_char(uint8_t *fb, int16_t x, int16_t y, char c, gfx_font_t font, uint8_t color) {
    const GFXfont *f = get_font(font);
    if (c < f->first || c > f->last) return 0;
    GFXglyph *glyph = &f->glyph[c - f->first];
    uint8_t *bitmap = f->bitmap;
    uint16_t bo = glyph->bitmapOffset;
    uint8_t gw = glyph->width, gh = glyph->height, xa = glyph->xAdvance;
    int8_t xo = glyph->xOffset, yo = glyph->yOffset;
    uint8_t bit = 0, bits = 0;
    for (uint8_t yy = 0; yy < gh; yy++) {
        for (uint8_t xx = 0; xx < gw; xx++) {
            if (!(bit++ & 7)) bits = bitmap[bo++];
            if (bits & 0x80) gfx_pixel(fb, x + xo + xx, y + yo + yy, color);
            bits <<= 1;
        }
    }
    return xa;
}

int16_t gfx_puts(uint8_t *fb, int16_t x, int16_t y, const char *str, gfx_font_t font, uint8_t color) {
    while (*str) x += gfx_draw_char(fb, x, y, *str++, font, color);
    return x;
}

int16_t gfx_text_width(const char *str, gfx_font_t font) {
    const GFXfont *f = get_font(font);
    int16_t w = 0;
    while (*str) {
        char c = *str++;
        if (c >= f->first && c <= f->last) w += f->glyph[c - f->first].xAdvance;
    }
    return w;
}

int16_t gfx_font_height(gfx_font_t font) {
    return get_font(font)->yAdvance;
}

void gfx_puts_centered(uint8_t *fb, int16_t y, const char *str, gfx_font_t font, uint8_t color) {
    int16_t w = gfx_text_width(str, font);
    gfx_puts(fb, (EPD_WIDTH - w) / 2, y, str, font, color);
}
