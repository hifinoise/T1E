#pragma once

#include <stdint.h>
#include <stdbool.h>

#define DS3231_ADDR     0x68

typedef struct {
    uint8_t  sec, min, hour;
    uint8_t  dow;
    uint8_t  day, month;
    uint16_t year;
} rtc_time_t;

bool rtc_init(void);
bool rtc_get_time(rtc_time_t *t);
bool rtc_set_time(const rtc_time_t *t);
bool rtc_set_epoch(uint32_t epoch);
uint32_t rtc_get_epoch(void);
float rtc_get_temperature(void);
bool rtc_lost_power(void);
