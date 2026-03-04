#include "mode_art.h"
#include "mode_common.h"
#include "epd_gfx.h"
#include "rtc_ds3231.h"
#include "esp_random.h"
#include <stdlib.h>
#include <stdio.h>

#define ART_ALGOS 5

static uint32_t art_rng;
static uint32_t art_rand(void) {
    art_rng ^= art_rng << 13;
    art_rng ^= art_rng >> 17;
    art_rng ^= art_rng << 5;
    return art_rng;
}

/* Algo 0: Truchet tiles — 10/10 simple diagonal weave */
static void art_truchet(uint8_t *fb) {
    int tile = 10;
    for (int y = 0; y < EPD_HEIGHT; y += tile) {
        for (int x = 0; x < EPD_WIDTH; x += tile) {
            if (art_rand() & 1) {
                for (int i = 0; i < tile; i++)
                    gfx_pixel(fb, x + i, y + i, GFX_BLACK);
            } else {
                for (int i = 0; i < tile; i++)
                    gfx_pixel(fb, x + tile - 1 - i, y + i, GFX_BLACK);
            }
        }
    }
}

/* Algo 1: Concentric shapes */
static void art_concentric(uint8_t *fb) {
    int cx = 60 + art_rand() % 80;
    int cy = 60 + art_rand() % 80;
    int count = 8 + art_rand() % 12;
    for (int i = count; i > 0; i--) {
        int r = i * (90 / count);
        // Draw circle outline
        for (int a = 0; a < 360; a++) {
            // Fixed-point sin/cos approximation
            int dx = (r * ((a < 180) ? (90 - ((a < 90) ? a : 180 - a)) : (((a < 270) ? a - 270 : 270 - (a - 180))) )) / 90;
            int dy = (r * ((a < 90 || a > 270) ? (((a < 90) ? a : 360 - a) - 0) : (180 - a < 90 ? 180 - a : a - 180))) / 90;
            // Simpler: just draw rectangle outlines
            (void)dx; (void)dy;
        }
        // Actually just draw rectangles — fast and looks great on e-paper
        if (art_rand() & 1) {
            gfx_rect(fb, cx - r, cy - r, r * 2, r * 2, GFX_BLACK);
        } else {
            // Diamond
            for (int j = 0; j < r; j++) {
                gfx_pixel(fb, cx + j, cy - r + j, GFX_BLACK);
                gfx_pixel(fb, cx + r - j, cy + j, GFX_BLACK);
                gfx_pixel(fb, cx - j, cy + r - j, GFX_BLACK);
                gfx_pixel(fb, cx - r + j, cy - j, GFX_BLACK);
            }
        }
    }
}

/* Algo 2: Circle packing */
static void art_circles(uint8_t *fb) {
    for (int i = 0; i < 50; i++) {
        int cx = 10 + art_rand() % 180;
        int cy = 10 + art_rand() % 180;
        int r = 3 + art_rand() % 20;
        // Outline circle
        for (int a = 0; a < 360; a++) {
            // Bresenham circle points
            int dx = 0, dy = r, d = 3 - 2 * r;
            while (dx <= dy) {
                gfx_pixel(fb, cx + dx, cy + dy, GFX_BLACK);
                gfx_pixel(fb, cx - dx, cy + dy, GFX_BLACK);
                gfx_pixel(fb, cx + dx, cy - dy, GFX_BLACK);
                gfx_pixel(fb, cx - dx, cy - dy, GFX_BLACK);
                gfx_pixel(fb, cx + dy, cy + dx, GFX_BLACK);
                gfx_pixel(fb, cx - dy, cy + dx, GFX_BLACK);
                gfx_pixel(fb, cx + dy, cy - dx, GFX_BLACK);
                gfx_pixel(fb, cx - dy, cy - dx, GFX_BLACK);
                if (d < 0) { d += 4 * dx + 6; }
                else { d += 4 * (dx - dy) + 10; dy--; }
                dx++;
            }
            break; // Only need one pass of Bresenham
        }
    }
}

/* Algo 3: Maze — random horizontal/vertical segments */
static void art_maze(uint8_t *fb) {
    for (int y = 0; y < 200; y += 10) {
        for (int x = 0; x < 200; x += 10) {
            if (art_rand() & 1)
                gfx_hline(fb, x, y, 10, GFX_BLACK);
            else
                gfx_vline(fb, x, y, 10, GFX_BLACK);
        }
    }
}

/* Algo 4: Stipple field — density varies by region */
static void art_stipple(uint8_t *fb) {
    for (int y = 0; y < 200; y += 2) {
        for (int x = 0; x < 200; x += 2) {
            // Density gradient: denser toward center
            int dx = x - 100, dy = y - 100;
            int dist = dx * dx + dy * dy;
            int threshold = 8000 + (art_rand() % 4000);
            if (dist < threshold) {
                if ((art_rand() & 7) < 3)
                    gfx_pixel(fb, x, y, GFX_BLACK);
            }
        }
    }
}

static const char *algo_names[] = {
    "TRUCHET", "CONCENTRIC", "CIRCLES", "MAZE", "STIPPLE"
};

void mode_art_render(uint8_t *fb, t1e_state_t *s) {
    gfx_clear(fb, GFX_WHITE);

    rtc_time_t t = {0};
    rtc_get_time(&t);
    uint32_t date_seed = t.year * 10000 + t.month * 100 + t.day;
    art_rng = date_seed ^ ((uint32_t)s->art_algo * 0xDEADBEEF) ^ 0x12345678;
    if (art_rng == 0) art_rng = 1;

    int algo = s->art_algo % ART_ALGOS;
    switch (algo) {
        case 0: art_truchet(fb);    break;
        case 1: art_concentric(fb); break;
        case 2: art_circles(fb);    break;
        case 3: art_maze(fb);       break;
        case 4: art_stipple(fb);    break;
    }

    // Label in bottom corner
    gfx_fill_rect(fb, 0, 186, 200, 14, GFX_WHITE);
    gfx_puts_centered(fb, 197, algo_names[algo], FONT_SMALL, GFX_BLACK);
    draw_corner_clock(fb);
}

void mode_art_action(t1e_state_t *s) {
    s->art_algo = (s->art_algo + 1) % ART_ALGOS;
}
