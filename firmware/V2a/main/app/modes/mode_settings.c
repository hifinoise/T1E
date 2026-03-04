#include "mode_settings.h"
#include "epd_gfx.h"
#include "rtc_ds3231.h"
#include "hal.h"
#include <stdio.h>

#define SET_ITEMS 7

// Must match state.h REFRESH_SPEED_COUNT / DEGHOST_IDX_COUNT
static const char *SPEED_LABELS[]   = {"100MS", "250MS", "500MS", "1S", "10S", "60S"};
static const char *DEGHOST_LABELS[] = {"5M", "15M", "30M", "OFF"};

void mode_settings_render(uint8_t *fb, t1e_state_t *s) {
    gfx_clear(fb, GFX_WHITE);
    gfx_puts_centered(fb, 22, "SETTINGS", FONT_MEDIUM, GFX_BLACK);
    gfx_hline(fb, 20, 30, 160, GFX_BLACK);

    int cursor = s->dice_preset % SET_ITEMS;

    for (int i = 0; i < SET_ITEMS; i++) {
        int16_t y = 46 + i * 22;
        char buf[24];

        switch (i) {
            case 0:
                snprintf(buf, sizeof(buf), "LIFE: %s",
                    s->life_clock_style == LIFE_STYLE_VANILLA ? "VANILLA" : "CHAOS");
                break;
            case 1:
                snprintf(buf, sizeof(buf), "LIFE GEN: %d", s->life_clock_gens);
                break;
            case 2:
                snprintf(buf, sizeof(buf), "REFRESH: %s",
                    SPEED_LABELS[s->refresh_speed % REFRESH_SPEED_COUNT]);
                break;
            case 3:
                snprintf(buf, sizeof(buf), "DEGHOST: %s",
                    DEGHOST_LABELS[s->deghost_idx % DEGHOST_IDX_COUNT]);
                break;
            case 4:
                snprintf(buf, sizeof(buf), "INVERT: %s",
                    s->display_invert ? "ON" : "OFF");
                break;
            case 5:
                snprintf(buf, sizeof(buf), "BATT: %d%%", hal_power_battery_percent());
                break;
            case 6:
                snprintf(buf, sizeof(buf), "EXIT");
                break;
            default:
                snprintf(buf, sizeof(buf), "?");
                break;
        }

        if (i == cursor) {
            gfx_fill_rect(fb, 4, y - 10, 192, 18, GFX_BLACK);
            gfx_puts(fb, 10, y, buf, FONT_SMALL, GFX_WHITE);
        } else {
            gfx_puts(fb, 10, y, buf, FONT_SMALL, GFX_BLACK);
        }
    }

    gfx_puts_centered(fb, 195, "A:OK  B:NEXT", FONT_SMALL, GFX_BLACK);
}

// B button: advance cursor
void mode_settings_action(t1e_state_t *s) {
    s->dice_preset = (s->dice_preset + 1) % SET_ITEMS;
}

// A button: activate selected item
void mode_settings_confirm(t1e_state_t *s) {
    int cursor = s->dice_preset % SET_ITEMS;
    switch (cursor) {
        case 0: // LIFE style
            s->life_clock_style = (s->life_clock_style + 1) % 2;
            break;
        case 1: // LIFE GEN count
            s->life_clock_gens = (s->life_clock_gens % 10) + 1;
            break;
        case 2: // REFRESH speed
            s->refresh_speed = (s->refresh_speed + 1) % REFRESH_SPEED_COUNT;
            break;
        case 3: // DEGHOST interval
            s->deghost_idx = (s->deghost_idx + 1) % DEGHOST_IDX_COUNT;
            break;
        case 4: // INVERT toggle
            s->display_invert = !s->display_invert;
            break;
        case 5: // BATT — display only, no action
            break;
        case 6: // EXIT
            s->mode = MODE_CLOCK;
            s->dice_preset = 0;
            break;
    }
}
