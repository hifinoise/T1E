#pragma once

#include <stdint.h>
#include <stdbool.h>

#define EPD_WIDTH       200
#define EPD_HEIGHT      200
#define EPD_FB_SIZE     (EPD_WIDTH * EPD_HEIGHT / 8)

typedef enum {
    EPD_REFRESH_FULL,
    EPD_REFRESH_PARTIAL,
} epd_refresh_t;

typedef struct {
    uint16_t x, y, w, h;
} epd_rect_t;

void epd_init(void);
void epd_sleep(void);
void epd_refresh_full(const uint8_t *fb);
void epd_refresh_partial(const uint8_t *old_fb, const uint8_t *new_fb);
void epd_refresh_window(const uint8_t *old_fb, const uint8_t *new_fb, epd_rect_t rect);
epd_refresh_t epd_refresh_smart(const uint8_t *old_fb, const uint8_t *new_fb,
                                 uint8_t *partial_count, bool force_full);
epd_refresh_t epd_refresh_animate(const uint8_t *old_fb, const uint8_t *new_fb,
                                   uint8_t *partial_count);
// Deghost: full refresh to all-white (clears image retention), then partial
// refresh back to fb. One white flash, then content reappears without a
// second full-screen drive cycle.
void epd_deghost(const uint8_t *fb);
epd_rect_t epd_diff_rect(const uint8_t *a, const uint8_t *b);
bool epd_is_busy(void);
void epd_wait_busy(void);
