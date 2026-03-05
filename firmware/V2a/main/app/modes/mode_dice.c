#include "mode_dice.h"
#include "mode_common.h"
#include "epd_gfx.h"
#include "rtc_ds3231.h"
#include "esp_random.h"
#include <stdio.h>

// Draw a classic die face for D6 (dot patterns)
static void draw_d6_face(uint8_t *fb, int val) {
    int cx = 100, cy = 100, sz = 60; // center and half-size
    int l = cx - sz, t = cy - sz;
    int dot = 8;

    // Die outline
    gfx_rect(fb, l, t, sz * 2, sz * 2, GFX_BLACK);
    gfx_rect(fb, l + 1, t + 1, sz * 2 - 2, sz * 2 - 2, GFX_BLACK);

    // Dot positions
    int px[3] = {cx - 30, cx, cx + 30};
    int py[3] = {cy - 30, cy, cy + 30};

    #define DOT(col,row) do { \
        for (int dy = -dot; dy <= dot; dy++) \
            for (int dx = -dot; dx <= dot; dx++) \
                if (dx*dx + dy*dy <= dot*dot) \
                    gfx_pixel(fb, px[col] + dx, py[row] + dy, GFX_BLACK); \
    } while(0)

    switch (val) {
        case 1: DOT(1,1); break;
        case 2: DOT(0,0); DOT(2,2); break;
        case 3: DOT(0,0); DOT(1,1); DOT(2,2); break;
        case 4: DOT(0,0); DOT(2,0); DOT(0,2); DOT(2,2); break;
        case 5: DOT(0,0); DOT(2,0); DOT(1,1); DOT(0,2); DOT(2,2); break;
        case 6: DOT(0,0); DOT(2,0); DOT(0,1); DOT(2,1); DOT(0,2); DOT(2,2); break;
    }
    #undef DOT
}

void mode_dice_render(uint8_t *fb, t1e_state_t *s) {
    gfx_clear(fb, GFX_WHITE);

    const char *name = dice_name(s->dice_type);

    if (s->dice_type == DICE_D6 && s->last_roll >= 1 && s->last_roll <= 6) {
        // Special visual for D6
        draw_d6_face(fb, s->last_roll);
        gfx_puts_centered(fb, 185, name, FONT_SMALL, GFX_BLACK);
    } else {
        // Label at top
        gfx_puts_centered(fb, 40, name, FONT_MEDIUM, GFX_BLACK);

        // Big number
        if (s->last_roll > 0) {
            char buf[8];
            snprintf(buf, sizeof(buf), "%d", s->last_roll);
            gfx_puts_centered(fb, 125, buf, FONT_LARGE, GFX_BLACK);
        } else {
            gfx_puts_centered(fb, 125, "?", FONT_LARGE, GFX_BLACK);
        }

        gfx_puts_centered(fb, 185, "B:ROLL", FONT_SMALL, GFX_BLACK);
    }

    draw_corner_clock(fb);
}

void mode_dice_action(t1e_state_t *s) {
    // Roll current die
    int max = dice_max_value(s->dice_type);
    s->last_roll = (esp_random() % max) + 1;
}
