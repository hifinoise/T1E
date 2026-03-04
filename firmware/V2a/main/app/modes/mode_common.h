#pragma once
#include <stdint.h>
#include "epd_gfx.h"
#include "rtc_ds3231.h"
#include <stdio.h>

// Draw a small clock in the top-left corner (used by all non-clock modes)
static inline void draw_corner_clock(uint8_t *fb) {
    rtc_time_t t = {0};
    if (!rtc_get_time(&t)) return;
    char ts[8];
    snprintf(ts, sizeof(ts), "%02d:%02d", t.hour, t.min);
    gfx_puts(fb, 2, 12, ts, FONT_SMALL, GFX_BLACK);
}
