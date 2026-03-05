#include "mode_pomo.h"
#include "mode_common.h"
#include "epd_gfx.h"
#include "rtc_ds3231.h"
#include <stdio.h>

#define POMO_WORK_MIN  25
#define POMO_BREAK_MIN 5

void mode_pomo_render(uint8_t *fb, t1e_state_t *s) {
    gfx_clear(fb, GFX_WHITE);

    if (s->pomo_start == 0) {
        // Idle state
        gfx_puts_centered(fb, 80, "POMODORO", FONT_LARGE, GFX_BLACK);

        char buf[16];
        snprintf(buf, sizeof(buf), "%d DONE", s->pomo_count);
        gfx_puts_centered(fb, 130, buf, FONT_MEDIUM, GFX_BLACK);

        gfx_puts_centered(fb, 185, "B:START", FONT_SMALL, GFX_BLACK);
    } else {
        // Active timer
        rtc_time_t t = {0};
        rtc_get_time(&t);
        uint32_t now_epoch = t.hour * 3600 + t.min * 60 + t.sec;
        int32_t elapsed = (int32_t)(now_epoch - s->pomo_start);
        if (elapsed < 0) elapsed += 86400;

        int total = POMO_WORK_MIN * 60;
        int remaining = total - elapsed;
        bool is_break = false;

        if (remaining <= 0) {
            int break_remaining = (POMO_WORK_MIN + POMO_BREAK_MIN) * 60 - elapsed;
            if (break_remaining <= 0) {
                s->pomo_count++;
                s->pomo_start = 0;
                mode_pomo_render(fb, s);
                return;
            }
            remaining = break_remaining;
            is_break = true;
        }

        // State label
        gfx_puts_centered(fb, 45, is_break ? "BREAK" : "FOCUS", FONT_MEDIUM, GFX_BLACK);

        // Big countdown
        int mn = remaining / 60;
        int sc = remaining % 60;
        char buf[12];
        snprintf(buf, sizeof(buf), "%02d:%02d", mn, sc);
        gfx_puts_centered(fb, 110, buf, FONT_LARGE, GFX_BLACK);

        // Progress bar
        int bar_w = 160;
        int bar_x = (200 - bar_w) / 2;
        int pct;
        if (is_break) {
            pct = ((POMO_BREAK_MIN * 60 - remaining) * bar_w) / (POMO_BREAK_MIN * 60);
        } else {
            pct = (elapsed * bar_w) / total;
        }
        if (pct < 0) pct = 0;
        if (pct > bar_w) pct = bar_w;
        gfx_rect(fb, bar_x, 145, bar_w, 12, GFX_BLACK);
        if (pct > 0) gfx_fill_rect(fb, bar_x + 1, 146, pct - 1, 10, GFX_BLACK);

        // Session counter
        char cnt[16];
        snprintf(cnt, sizeof(cnt), "#%d", s->pomo_count + 1);
        gfx_puts_centered(fb, 175, cnt, FONT_SMALL, GFX_BLACK);

        gfx_puts_centered(fb, 195, "B:CANCEL", FONT_SMALL, GFX_BLACK);
    }

    draw_corner_clock(fb);
}

void mode_pomo_action(t1e_state_t *s) {
    if (s->pomo_start == 0) {
        rtc_time_t t = {0};
        rtc_get_time(&t);
        s->pomo_start = t.hour * 3600 + t.min * 60 + t.sec;
    } else {
        s->pomo_start = 0;
    }
}
