#include "mode_life_clock.h"
#include "mode_common.h"
#include "epd_gfx.h"
#include "rtc_ds3231.h"
#include "esp_random.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 40x40 grid, 5px per cell = 200x200 display
#define GRID 40
#define CELL 5

static uint8_t grid[GRID][GRID];
static uint8_t next_g[GRID][GRID];

static int neighbors(int x, int y) {
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = (x + dx + GRID) % GRID;
            int ny = (y + dy + GRID) % GRID;
            if (grid[ny][nx]) n++;
        }
    return n;
}

static void step(void) {
    for (int y = 0; y < GRID; y++)
        for (int x = 0; x < GRID; x++) {
            int n = neighbors(x, y);
            next_g[y][x] = grid[y][x] ? (n == 2 || n == 3) : (n == 3);
        }
    memcpy(grid, next_g, sizeof(grid));
}

// 5x7 digit outlines — single-pixel-wide strokes that interact 
// better with Life rules (solid blocks die instantly from overpopulation)
static const uint8_t digit5x7[10][7] = {
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, // 0
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}, // 1
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F}, // 2
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E}, // 3
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}, // 4
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E}, // 5
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E}, // 6
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}, // 7
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}, // 8
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}, // 9
};

static void stamp_digit(int d, int gx, int gy) {
    if (d < 0 || d > 9) return;
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (digit5x7[d][row] & (0x10 >> col))
                grid[gy + row][gx + col] = 1;
}

static void stamp_colon(int gx, int gy) {
    grid[gy + 2][gx] = 1;
    grid[gy + 4][gx] = 1;
}

// Clear a 2-cell margin around the time digit area so Life cells don't
// bleed directly up against the digits. Digits span x=8..32, y=16..22 (cells).
static void clear_time_zone(void) {
    for (int y = 14; y <= 24; y++)
        for (int x = 6; x <= 34; x++)
            if (x < GRID && y < GRID)
                grid[y][x] = 0;
}

static void inject_time(uint8_t h, uint8_t m) {
    int ox = 8, oy = 16;
    clear_time_zone();
    stamp_digit(h / 10, ox, oy);
    stamp_digit(h % 10, ox + 6, oy);
    stamp_colon(ox + 12, oy);
    stamp_digit(m / 10, ox + 14, oy);
    stamp_digit(m % 10, ox + 20, oy);
}

// Sprinkle random cells around the digits so the grid doesn't die
static void sprinkle_noise(void) {
    for (int i = 0; i < 80; i++) {
        int x = esp_random() % GRID;
        int y = esp_random() % GRID;
        grid[y][x] = 1;
    }
}

static void grid_to_fb(uint8_t *fb, bool invert) {
    if (invert) {
        gfx_clear(fb, GFX_BLACK);
        for (int y = 0; y < GRID; y++)
            for (int x = 0; x < GRID; x++)
                if (grid[y][x])
                    gfx_fill_rect(fb, x * CELL, y * CELL, CELL, CELL, GFX_WHITE);
    } else {
        gfx_clear(fb, GFX_WHITE);
        for (int y = 0; y < GRID; y++)
            for (int x = 0; x < GRID; x++)
                if (grid[y][x])
                    gfx_fill_rect(fb, x * CELL, y * CELL, CELL, CELL, GFX_BLACK);
    }
}

// Styles: 0=vanilla, 1=chaos, 2=inverted
#define LIFE_CLOCK_STYLES 3

void mode_life_clock_render(uint8_t *fb, t1e_state_t *s) {
    rtc_time_t t = {0};
    rtc_get_time(&t);

    int style = s->life_clock_style % LIFE_CLOCK_STYLES;

    if (style == 0) {
        // Vanilla: clear, stamp time, add some noise, evolve gently
        memset(grid, 0, sizeof(grid));
        inject_time(t.hour, t.min);
        sprinkle_noise();
        for (int i = 0; i < s->life_clock_gens; i++) step();
        // Re-stamp so time is always readable
        inject_time(t.hour, t.min);
        grid_to_fb(fb, false);
    } else if (style == 1) {
        // Chaos: persistent soup, evolve, re-stamp
        if (s->life_seed == 0) {
            for (int y = 0; y < GRID; y++)
                for (int x = 0; x < GRID; x++)
                    grid[y][x] = (esp_random() & 3) == 0;
            s->life_seed = 1;
        }
        for (int i = 0; i < s->life_clock_gens; i++) step();
        // If grid is mostly dead, revive it
        int alive = 0;
        for (int y = 0; y < GRID; y++)
            for (int x = 0; x < GRID; x++)
                alive += grid[y][x];
        if (alive < 40) sprinkle_noise();
        inject_time(t.hour, t.min);
        grid_to_fb(fb, false);
    } else {
        // Inverted: same as chaos but white-on-black
        if (s->life_seed == 0) {
            for (int y = 0; y < GRID; y++)
                for (int x = 0; x < GRID; x++)
                    grid[y][x] = (esp_random() & 3) == 0;
            s->life_seed = 1;
        }
        for (int i = 0; i < s->life_clock_gens; i++) step();
        int alive = 0;
        for (int y = 0; y < GRID; y++)
            for (int x = 0; x < GRID; x++)
                alive += grid[y][x];
        if (alive < 40) sprinkle_noise();
        inject_time(t.hour, t.min);
        grid_to_fb(fb, true);
    }
}

void mode_life_clock_action(t1e_state_t *s) {
    s->life_clock_style = (s->life_clock_style + 1) % LIFE_CLOCK_STYLES;
    s->life_seed = 0; // reset grid on style change
}
