#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "hal/hal.h"
#include "hal/hal_pins.h"
#include "drivers/epd/epd.h"
#include "drivers/epd/epd_gfx.h"
#include "drivers/rtc/rtc_ds3231.h"
#include "app/state.h"
#include "app/modes/mode_clock.h"
#include "app/modes/mode_life_clock.h"
#include "app/modes/mode_dice.h"
#include "app/modes/mode_art.h"
#include "app/modes/mode_pomo.h"
#include "app/modes/mode_life.h"
#include "app/modes/mode_settings.h"

static const char *TAG = "t1e";

// Framebuffers in regular SRAM — NOT RTC slow memory
static uint8_t fb[EPD_FB_SIZE];
static uint8_t prev_fb[EPD_FB_SIZE];

static void render(t1e_state_t *s) {
    switch (s->mode) {
        case MODE_CLOCK:      mode_clock_render(fb, s);      break;
        case MODE_LIFE_CLOCK: mode_life_clock_render(fb, s); break;
        case MODE_DICE:       mode_dice_render(fb, s);       break;
        case MODE_ART:        mode_art_render(fb, s);        break;
        case MODE_POMO:       mode_pomo_render(fb, s);       break;
        case MODE_LIFE:       mode_life_render(fb, s);       break;
        case MODE_SETTINGS:   mode_settings_render(fb, s);   break;
        default:              mode_clock_render(fb, s);       break;
    }
}

static void action(t1e_state_t *s) {
    switch (s->mode) {
        case MODE_CLOCK:      mode_clock_action(s);      break;
        case MODE_LIFE_CLOCK: mode_life_clock_action(s); break;
        case MODE_DICE:       mode_dice_action(s);       break;
        case MODE_ART:        mode_art_action(s);        break;
        case MODE_POMO:       mode_pomo_action(s);       break;
        case MODE_LIFE:       mode_life_action(s);       break;
        case MODE_SETTINGS:   mode_settings_action(s);   break;
        default: break;
    }
}

static void settings_confirm(t1e_state_t *s) {
    int cursor = s->dice_preset % 4;
    switch (cursor) {
        case 0: s->life_clock_style = (s->life_clock_style + 1) % 3; break;
        case 1: s->life_clock_gens = (s->life_clock_gens % 10) + 1; break;
        case 2: break;
        case 3: s->mode = MODE_CLOCK; s->dice_preset = 0; break;
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== T1E BOOT ===");

    hal_buttons_init();
    hal_power_init();
    if (!rtc_init()) ESP_LOGE(TAG, "RTC FAILED");
    epd_init();

    t1e_state_t *s = state_get();
    if (s->magic != STATE_MAGIC) {
        state_init_defaults(s);
    }

    // Cold boot: full refresh to establish clean baseline in both RAMs
    render(s);
    epd_refresh_full(fb);
    memcpy(prev_fb, fb, EPD_FB_SIZE);

    uint32_t last_refresh = 0;
    uint32_t ghost_timer = xTaskGetTickCount() / configTICK_RATE_HZ;
    const uint32_t DEGHOST_S = 300;

    while (1) {
        btn_state_t btn = hal_buttons_read();
        bool need_refresh = false;
        bool force_full = false;

        if (btn.a) {
            if (s->mode == MODE_SETTINGS) {
                settings_confirm(s);
            } else if (s->mode == MODE_DICE) {
                if (s->dice_type < DICE_COUNT - 1) {
                    s->dice_type++;
                    s->last_roll = 0;
                } else {
                    s->dice_type = DICE_D20;
                    s->last_roll = 0;
                    state_next_mode(s);
                }
            } else {
                state_next_mode(s);
            }
            need_refresh = true;
        }

        if (btn.b) {
            action(s);
            need_refresh = true;
        }

        uint32_t now = xTaskGetTickCount() / configTICK_RATE_HZ;
        int interval = 60;
        if (s->mode == MODE_LIFE) interval = 1;
        if (s->mode == MODE_LIFE_CLOCK) interval = 1;
        if (s->mode == MODE_POMO && s->pomo_start) interval = 1;
        if (now - last_refresh >= (uint32_t)interval) {
            need_refresh = true;
            last_refresh = now;
        }

        if (now - ghost_timer >= DEGHOST_S) {
            force_full = true;
            need_refresh = true;
            ghost_timer = now;
        }

        if (need_refresh) {
            render(s);

            if (force_full) {
                epd_refresh_full(fb);
                s->partial_count = 0;
            } else {
                epd_refresh_animate(prev_fb, fb, &s->partial_count);
            }
            memcpy(prev_fb, fb, EPD_FB_SIZE);
        }

        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
