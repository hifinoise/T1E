#include "state.h"
#include "esp_attr.h"
#include <string.h>

// Stored in RTC slow memory — persists across deep sleep
static RTC_DATA_ATTR t1e_state_t s_state;

t1e_state_t* state_get(void) { return &s_state; }

void state_init_defaults(t1e_state_t *s) {
    memset(s, 0, sizeof(*s));
    s->magic = STATE_MAGIC;
    s->mode = MODE_CLOCK;
    s->clock_face = 0;
    s->life_clock_style = LIFE_STYLE_VANILLA;
    s->life_clock_gens = 3;
    s->dice_type = DICE_D20;
    s->mode_enabled = 0xFF; // all modes on
    s->refresh_speed = 4;   // 60s default
    s->deghost_idx   = 1;   // 15min default
    s->display_invert = 0;  // normal
}

t1e_mode_t state_next_mode(t1e_state_t *s) {
    t1e_mode_t start = s->mode;
    do {
        s->mode = (s->mode + 1) % MODE_COUNT;
    } while (!state_mode_enabled(s, s->mode) && s->mode != start);
    return s->mode;
}

bool state_mode_enabled(t1e_state_t *s, t1e_mode_t mode) {
    if (mode == MODE_CLOCK || mode == MODE_SETTINGS) return true; // always on
    return (s->mode_enabled >> mode) & 1;
}

int dice_max_value(dice_type_t type) {
    static const int vals[] = {4, 6, 8, 10, 12, 20, 100};
    if (type < DICE_COUNT) return vals[type];
    return 6;
}

const char* dice_name(dice_type_t type) {
    static const char *names[] = {"D4","D6","D8","D10","D12","D20","D100"};
    if (type < DICE_COUNT) return names[type];
    return "D6";
}
