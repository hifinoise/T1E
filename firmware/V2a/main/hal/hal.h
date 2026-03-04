#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool a;
    bool b;
    bool both;
} btn_state_t;

void hal_buttons_init(void);
btn_state_t hal_buttons_read(void);

void hal_sleep_init(void);
void hal_sleep_enter(uint32_t sleep_seconds);
bool hal_sleep_was_timer_wake(void);

void hal_power_init(void);
float hal_power_battery_voltage(void);
int hal_power_battery_percent(void);
bool hal_power_is_critical(void);
