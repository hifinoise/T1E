#include "mode_clock.h"
#include "epd_gfx.h"
#include "rtc_ds3231.h"
#include "hal.h"
#include <stdio.h>

// Battery icon — matches original T1E.ino drawBatteryIcon()
// Original: x=160 y=0, rect(x,y,30,15), terminal(x+31,y+4,2,6)
// fillWidth = 30 * battPercent / 100
static void draw_battery(uint8_t *fb, int pct, float voltage, uint8_t color) {
    int x = 160, y = 0;

    // Battery outline (30x15) + terminal nub
    gfx_rect(fb, x, y, 30, 15, color);
    gfx_fill_rect(fb, x + 31, y + 4, 2, 6, color);

    // Fill bar — original: (int)(30 * battPercent / 100)
    int fill = (30 * pct) / 100;
    if (fill < 0) fill = 0;
    if (fill > 28) fill = 28;
    if (fill > 0)
        gfx_fill_rect(fb, x + 1, y + 1, fill, 13, color);

    // Voltage + percentage text — from T1E_Battery_C3.ino
    char vbuf[16];
    snprintf(vbuf, sizeof(vbuf), "%.1fV", (double)voltage);
    gfx_puts(fb, 120, 35, vbuf, FONT_SMALL, color);

    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%d%%", pct);
    gfx_puts(fb, 142, 55, pbuf, FONT_SMALL, color);

    // Low battery warning
    if (voltage < 3.2f) {
        gfx_puts(fb, 140, 75, "LOW", FONT_SMALL, color);
    }
}

/* Face 0: Full — time, date, battery (original layout) */
static void face_full(uint8_t *fb) {
    rtc_time_t t = {0};
    if (!rtc_get_time(&t)) {
        gfx_puts_centered(fb, 110, "NO RTC", FONT_MEDIUM, GFX_BLACK);
        return;
    }

    char ts[8];
    snprintf(ts, sizeof(ts), "%02d:%02d", t.hour, t.min);
    gfx_puts_centered(fb, 100, ts, FONT_LARGE, GFX_BLACK);

    static const char *mon[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    int mi = t.month;
    if (mi < 1 || mi > 12) mi = 1;
    char ds[20];
    snprintf(ds, sizeof(ds), "%02d %s %04d", t.day, mon[mi - 1], t.year);
    gfx_puts_centered(fb, 150, ds, FONT_MEDIUM, GFX_BLACK);

    float v = hal_power_battery_voltage();
    int pct = hal_power_battery_percent();
    draw_battery(fb, pct, v, GFX_BLACK);
}

/* Face 1: Minimal — just time */
static void face_minimal(uint8_t *fb) {
    rtc_time_t t = {0};
    if (!rtc_get_time(&t)) {
        gfx_puts_centered(fb, 110, "NO RTC", FONT_MEDIUM, GFX_BLACK);
        return;
    }
    char ts[8];
    snprintf(ts, sizeof(ts), "%02d:%02d", t.hour, t.min);
    gfx_puts_centered(fb, 115, ts, FONT_LARGE, GFX_BLACK);
}

/* Face 2: Inverted — white on black (T1E_Battery_C3 style) */
static void face_inverted(uint8_t *fb) {
    gfx_clear(fb, GFX_BLACK);
    rtc_time_t t = {0};
    if (!rtc_get_time(&t)) {
        gfx_puts_centered(fb, 110, "NO RTC", FONT_MEDIUM, GFX_WHITE);
        return;
    }

    char ts[8];
    snprintf(ts, sizeof(ts), "%02d:%02d", t.hour, t.min);
    gfx_puts_centered(fb, 100, ts, FONT_LARGE, GFX_WHITE);

    static const char *mon[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
    };
    int mi = t.month;
    if (mi < 1 || mi > 12) mi = 1;
    char ds[20];
    snprintf(ds, sizeof(ds), "%02d %s", t.day, mon[mi - 1]);
    gfx_puts_centered(fb, 150, ds, FONT_MEDIUM, GFX_WHITE);

    float v = hal_power_battery_voltage();
    int pct = hal_power_battery_percent();
    draw_battery(fb, pct, v, GFX_WHITE);
}

#define CLOCK_FACES 3

void mode_clock_render(uint8_t *fb, t1e_state_t *s) {
    gfx_clear(fb, GFX_WHITE);
    switch (s->clock_face % CLOCK_FACES) {
        case 0: face_full(fb);     break;
        case 1: face_minimal(fb);  break;
        case 2: face_inverted(fb); break;
    }
}

void mode_clock_action(t1e_state_t *s) {
    s->clock_face = (s->clock_face + 1) % CLOCK_FACES;
}
