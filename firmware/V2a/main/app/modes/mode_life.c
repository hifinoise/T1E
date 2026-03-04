#include "mode_life.h"
#include "mode_common.h"
#include "epd_gfx.h"
#include "esp_random.h"
#include <string.h>
#include <stdio.h>

// 50x50 grid, 4px per cell = 200x200 display. Visible. Not a QR code.
#define GRID_W 50
#define GRID_H 50
#define CELL   4

static uint8_t grid[GRID_H][GRID_W];
static uint8_t next_grid[GRID_H][GRID_W];

static int neighbors(int x, int y) {
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = (x + dx + GRID_W) % GRID_W;
            int ny = (y + dy + GRID_H) % GRID_H;
            if (grid[ny][nx]) n++;
        }
    return n;
}

static void step(void) {
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++) {
            int n = neighbors(x, y);
            next_grid[y][x] = grid[y][x] ? (n == 2 || n == 3) : (n == 3);
        }
    memcpy(grid, next_grid, sizeof(grid));
}

static void grid_to_fb(uint8_t *fb) {
    gfx_clear(fb, GFX_WHITE);
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++)
            if (grid[y][x])
                gfx_fill_rect(fb, x * CELL, y * CELL, CELL, CELL, GFX_BLACK);
}

#define GENS_PER_FRAME 3

void mode_life_render(uint8_t *fb, t1e_state_t *s) {
    if (s->life_gen == 0) {
        // Seed with hardware RNG
        memset(grid, 0, sizeof(grid));
        for (int y = 10; y < 40; y++)
            for (int x = 10; x < 40; x++)
                grid[y][x] = (esp_random() & 3) == 0;
        s->life_gen = 1;
    } else {
        for (int g = 0; g < GENS_PER_FRAME; g++) {
            step();
            s->life_gen++;
        }
    }

    grid_to_fb(fb);

    // Gen counter overlay
    char buf[16];
    snprintf(buf, sizeof(buf), "G%d", s->life_gen);
    gfx_fill_rect(fb, 0, 0, 70, 16, GFX_WHITE);
    gfx_puts(fb, 2, 13, buf, FONT_SMALL, GFX_BLACK);
}

void mode_life_action(t1e_state_t *s) {
    s->life_gen = 0;
    s->life_seed = esp_random();
}
