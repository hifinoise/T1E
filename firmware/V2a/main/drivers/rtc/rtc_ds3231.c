#include "rtc_ds3231.h"
#include "hal_pins.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "ds3231";
static i2c_master_bus_handle_t bus_handle = NULL;
static i2c_master_dev_handle_t dev_handle = NULL;

static uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10 + (b & 0x0F); }
static uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

static bool i2c_read(uint8_t reg, uint8_t *buf, size_t len) {
    return i2c_master_transmit_receive(dev_handle, &reg, 1, buf, len, 100) == ESP_OK;
}

static bool i2c_write(uint8_t reg, const uint8_t *data, size_t len) {
    uint8_t buf[len + 1];
    buf[0] = reg;
    for (size_t i = 0; i < len; i++) buf[i + 1] = data[i];
    return i2c_master_transmit(dev_handle, buf, len + 1, 100) == ESP_OK;
}

bool rtc_init(void) {
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_PORT,
        .sda_io_num = PIN_I2C_SDA,
        .scl_io_num = PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &bus_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        return false;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DS3231_ADDR,
        .scl_speed_hz = I2C_FREQ_HZ,
    };
    err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(err));
        return false;
    }

    uint8_t test;
    if (!i2c_read(0x00, &test, 1)) {
        ESP_LOGE(TAG, "DS3231 not responding");
        return false;
    }
    ESP_LOGI(TAG, "DS3231 OK");
    return true;
}

bool rtc_get_time(rtc_time_t *t) {
    uint8_t r[7];
    if (!i2c_read(0x00, r, 7)) return false;
    t->sec   = bcd2dec(r[0] & 0x7F);
    t->min   = bcd2dec(r[1] & 0x7F);
    t->hour  = bcd2dec(r[2] & 0x3F);
    t->dow   = r[3] & 0x07;
    t->day   = bcd2dec(r[4] & 0x3F);
    t->month = bcd2dec(r[5] & 0x1F);
    t->year  = 2000 + bcd2dec(r[6]);
    return true;
}

bool rtc_set_time(const rtc_time_t *t) {
    uint8_t r[7] = {
        dec2bcd(t->sec), dec2bcd(t->min), dec2bcd(t->hour),
        t->dow, dec2bcd(t->day), dec2bcd(t->month), dec2bcd(t->year - 2000),
    };
    return i2c_write(0x00, r, 7);
}

bool rtc_set_epoch(uint32_t epoch) {
    time_t raw = (time_t)epoch;
    struct tm *tm = gmtime(&raw);
    rtc_time_t t = {
        .sec = tm->tm_sec, .min = tm->tm_min, .hour = tm->tm_hour,
        .dow = tm->tm_wday + 1, .day = tm->tm_mday,
        .month = tm->tm_mon + 1, .year = tm->tm_year + 1900,
    };
    return rtc_set_time(&t);
}

uint32_t rtc_get_epoch(void) {
    rtc_time_t t;
    if (!rtc_get_time(&t)) return 0;
    struct tm tm = {
        .tm_sec = t.sec, .tm_min = t.min, .tm_hour = t.hour,
        .tm_mday = t.day, .tm_mon = t.month - 1, .tm_year = t.year - 1900,
    };
    return (uint32_t)mktime(&tm);
}

float rtc_get_temperature(void) {
    uint8_t r[2];
    if (!i2c_read(0x11, r, 2)) return -99.0f;
    int16_t raw = ((int16_t)(int8_t)r[0] << 2) | (r[1] >> 6);
    return raw * 0.25f;
}

bool rtc_lost_power(void) {
    uint8_t status;
    if (!i2c_read(0x0F, &status, 1)) return true;
    return (status & 0x80) != 0;
}
