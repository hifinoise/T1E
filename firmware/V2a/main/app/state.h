#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "epd.h"

#define STATE_MAGIC 0xA5

typedef enum {
    MODE_CLOCK = 0, MODE_LIFE_CLOCK, MODE_DICE,
    MODE_ART, MODE_POMO, MODE_LIFE, MODE_SETTINGS, MODE_COUNT,
} t1e_mode_t;

typedef enum {
    DICE_D4=0, DICE_D6, DICE_D8, DICE_D10,
    DICE_D12, DICE_D20, DICE_D100, DICE_COUNT,
} dice_type_t;

typedef enum { LIFE_STYLE_VANILLA=0, LIFE_STYLE_CHAOS, LIFE_STYLE_INVERTED } life_style_t;

// refresh_speed index → ms: {100, 250, 500, 1000, 10000, 60000}
#define REFRESH_SPEED_COUNT 6
// deghost_idx → ms between deghost full refresh: {300000, 900000, 1800000, 0=off}
#define DEGHOST_IDX_COUNT   4

typedef struct {
    uint8_t     magic;
    uint8_t     mode;
    uint8_t     partial_count;
    uint8_t     clock_face;
    uint8_t     life_clock_style;
    uint8_t     life_clock_gens;
    uint8_t     dice_type;
    uint8_t     last_roll;
    uint8_t     dice_preset;
    uint8_t     life_gen;
    uint32_t    life_seed;
    uint32_t    pomo_start;
    uint8_t     pomo_count;
    uint32_t    art_date;
    uint8_t     art_algo;
    uint32_t    last_sync;
    uint8_t     mode_enabled;
    uint8_t     refresh_speed;   // index into {1,5,10,30,60}s — general update interval
    uint8_t     deghost_idx;     // index into {5m,15m,30m,off} — periodic full-white refresh
    uint8_t     display_invert;  // 0=normal, 1=invert all pixels
    uint8_t     prev_fb[EPD_FB_SIZE]; // previous framebuffer for diff-based refresh
} t1e_state_t;

t1e_state_t* state_get(void);
void state_init_defaults(t1e_state_t *s);
t1e_mode_t state_next_mode(t1e_state_t *s);
bool state_mode_enabled(t1e_state_t *s, t1e_mode_t mode);
int dice_max_value(dice_type_t type);
const char* dice_name(dice_type_t type);
