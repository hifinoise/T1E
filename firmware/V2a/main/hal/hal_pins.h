#pragma once

// T1E Pin Definitions
// The PCB routes XIAO module pads to peripherals differently per variant.
// Arduino code used raw GPIO numbers — we do the same here in ESP-IDF.

#include "hal/spi_types.h"
#include "driver/i2c_types.h"

#ifdef BOARD_VARIANT_C3
// ---- XIAO ESP32-C3 (from T1E_Battery_C3.ino) ----
// Default SPI: MOSI=GPIO10(D10), SCK=GPIO8(D8)
#define PIN_SPI_MOSI    10
#define PIN_SPI_CLK     8
#define PIN_EPD_CS      5       // D3
#define PIN_EPD_DC      20      // D7
#define PIN_EPD_RST     21      // D6
#define PIN_EPD_BUSY    9       // D9
#define EPD_SPI_HOST    SPI2_HOST

// I2C: SDA=GPIO6(D4), SCL=GPIO7(D5)
#define PIN_I2C_SDA     6
#define PIN_I2C_SCL     7
#define I2C_PORT        I2C_NUM_0

// Buttons
#define PIN_BTN_A       3       // D1
#define PIN_BTN_B       4       // D2

// Battery ADC
#define PIN_BATT_ADC    2       // D0/A0
#define BATT_ADC_CHAN   ADC_CHANNEL_2

#else
// ---- XIAO ESP32-C6 (from T1E.ino) ----
// Default SPI: MOSI=GPIO18(D10), SCK=GPIO19(D8), MISO=GPIO20(D9)
#define PIN_SPI_MOSI    18
#define PIN_SPI_CLK     19
#define PIN_EPD_CS      21      // D3
#define PIN_EPD_DC      17      // D7
#define PIN_EPD_RST     16      // D6
#define PIN_EPD_BUSY    20      // D9 — shared with MISO pad but used as GPIO
#define EPD_SPI_HOST    SPI2_HOST

// I2C: SDA=GPIO22(D4), SCL=GPIO23(D5)
#define PIN_I2C_SDA     22
#define PIN_I2C_SCL     23
#define I2C_PORT        I2C_NUM_0

// Buttons
#define PIN_BTN_A       1       // D1
#define PIN_BTN_B       2       // D2

// Battery ADC
#define PIN_BATT_ADC    0       // D0/A0
#define BATT_ADC_CHAN   ADC_CHANNEL_0

#endif

// Common
#define I2C_FREQ_HZ     100000
#define DS3231_ADDR     0x68
#define EPD_WIDTH       200
#define EPD_HEIGHT      200
#define EPD_FB_SIZE     (EPD_WIDTH * EPD_HEIGHT / 8)
