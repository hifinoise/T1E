#include "hal.h"
#include "hal_pins.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "sleep";

void hal_sleep_init(void) { }

bool hal_sleep_was_timer_wake(void) {
    return (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER);
}

void hal_sleep_enter(uint32_t sleep_seconds) {
    ESP_LOGI(TAG, "sleeping %lus", (unsigned long)sleep_seconds);

    // Ensure button pullups are enabled before sleep
    // The GPIO config from hal_buttons_init may not persist into deep sleep
    // on all chips — re-enable pullups here to be safe.
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << PIN_BTN_A) | (1ULL << PIN_BTN_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_cfg);

    // Hold the GPIO config through deep sleep so pullups stay active
    gpio_hold_en(PIN_BTN_A);
    gpio_hold_en(PIN_BTN_B);

    // Timer wake
    esp_sleep_enable_timer_wakeup((uint64_t)sleep_seconds * 1000000ULL);

    // Button wake — LOW = pressed (pulled up by internal resistor)
    esp_deep_sleep_enable_gpio_wakeup(
        (1ULL << PIN_BTN_A) | (1ULL << PIN_BTN_B),
        ESP_GPIO_WAKEUP_GPIO_LOW
    );

    esp_deep_sleep_start();
}
