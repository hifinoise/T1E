#include "hal.h"
#include "hal_pins.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "btn";

static bool prev_a = false;
static bool prev_b = false;

void hal_buttons_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << PIN_BTN_A) | (1ULL << PIN_BTN_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    // Read initial state so we don't fire on boot
    vTaskDelay(pdMS_TO_TICKS(50));
    prev_a = (gpio_get_level(PIN_BTN_A) == 0);
    prev_b = (gpio_get_level(PIN_BTN_B) == 0);
}

btn_state_t hal_buttons_read(void) {
    bool raw_a = (gpio_get_level(PIN_BTN_A) == 0);
    bool raw_b = (gpio_get_level(PIN_BTN_B) == 0);

    // Edge detection: only fire on press (transition from not-pressed to pressed)
    bool edge_a = raw_a && !prev_a;
    bool edge_b = raw_b && !prev_b;

    prev_a = raw_a;
    prev_b = raw_b;

    btn_state_t state = {
        .a = edge_a && !raw_b,
        .b = edge_b && !raw_a,
        .both = edge_a && edge_b,
    };

    if (state.a || state.b || state.both) {
        ESP_LOGI(TAG, "PRESS a=%d b=%d", state.a, state.b);
    }

    return state;
}
