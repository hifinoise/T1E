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

// refresh_speed index → update interval in milliseconds
static const uint32_t REFRESH_MS[REFRESH_SPEED_COUNT] = {100, 250, 500, 1000, 10000, 60000};

// deghost_idx → ms between full-white deghost refresh (0 = disabled)
static const uint32_t DEGHOST_MS[DEGHOST_IDX_COUNT] = {5*60*1000, 15*60*1000, 30*60*1000, 0};

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
    if (s->display_invert)
        for (int i = 0; i < EPD_FB_SIZE; i++) fb[i] ^= 0xFF;
    epd_refresh_full(fb);
    memcpy(prev_fb, fb, EPD_FB_SIZE);

    uint32_t last_refresh = xTaskGetTickCount();
    uint32_t ghost_timer  = xTaskGetTickCount();

    while (1) {
        btn_state_t btn = hal_buttons_read();
        bool need_refresh = false;
        bool force_full   = false;
        bool do_deghost   = false;

        // --- Button A: secondary action ---
        if (btn.a) {
            action(s);
            need_refresh = true;
        }

        // --- Button B: confirm / mode advance ---
        if (btn.b) {
            if (s->mode == MODE_SETTINGS) {
                uint8_t invert_before = s->display_invert;
                mode_settings_confirm(s);
                // If invert just toggled, force a full refresh to reset RAM baseline
                if (s->display_invert != invert_before) force_full = true;
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

        // --- Time-based refresh ---
        uint32_t now = xTaskGetTickCount(); // ticks (1 tick = 1ms at default ESP32 config)
        uint32_t interval_ticks = pdMS_TO_TICKS(REFRESH_MS[s->refresh_speed % REFRESH_SPEED_COUNT]);
        // POMO countdown always needs 1s resolution
        if (s->mode == MODE_POMO && s->pomo_start) interval_ticks = pdMS_TO_TICKS(1000);

        static int last_min = -1;
        if (s->mode == MODE_CLOCK || s->mode == MODE_LIFE_CLOCK) {
            // For clock modes, only refresh when the minute actually changes.
            // We ignore interval_ticks entirely, but poll gently every 1 second.
            if (now - last_refresh >= pdMS_TO_TICKS(1000)) {
                rtc_time_t t = {0};
                if (rtc_get_time(&t)) {
                    if (t.min != last_min) {
                        need_refresh = true;
                        last_min = t.min;
                    }
                }
                last_refresh = now;
            }
        } else {
            // Normal global refresh interval for non-clock modes
            if (now - last_refresh >= interval_ticks) {
                need_refresh = true;
                last_refresh = now;
            }
        }

        // --- Deghost watchdog ---
        // Banks display to all-white then partial-refreshes back to current frame.
        // This clears image retention without a jarring double-flash.
        uint32_t dghost_ms = DEGHOST_MS[s->deghost_idx % DEGHOST_IDX_COUNT];
        if (dghost_ms > 0 && now - ghost_timer >= pdMS_TO_TICKS(dghost_ms)) {
            do_deghost   = true;
            need_refresh = true;
            ghost_timer  = now;
        }

        // --- Render and refresh ---
        if (need_refresh) {
            render(s);

            // Apply inversion if enabled
            if (s->display_invert)
                for (int i = 0; i < EPD_FB_SIZE; i++) fb[i] ^= 0xFF;

            if (do_deghost) {
                // Full white refresh to clear ghosting, then partial back to frame
                epd_deghost(fb);
                s->partial_count = 0;
            } else if (force_full) {
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
