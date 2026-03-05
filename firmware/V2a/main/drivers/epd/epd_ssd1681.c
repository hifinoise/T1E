// SSD1681 driver for GDEY0154D67 (200×200 B/W)
//
// Partial refresh approach (confirmed working on hardware):
//
//   A custom waveform LUT is loaded via register 0x32 before each partial
//   update. Without this, the controller always falls back to the OTP
//   full-refresh waveform regardless of the 0x22 mode byte — causing a
//   full blank flash every time.
//
//   0x22 = 0xC7 triggers the update cycle using the loaded LUT:
//     0xF7 = enable clock+analog, load temp, load OTP LUT, display, disable
//     0xC7 = same sequence but skips OTP LUT load → uses our custom LUT
//
//   The LUT is differential: unchanged pixels (LL, HH) get GND (no voltage,
//   no movement). Only changed pixels (LH, HL) receive a drive pulse.
//   This eliminates the full-screen flash.
//
//   HW reset is NOT used before partials. A reset clears all registers
//   including any loaded LUT, forcing a fallback to OTP on the next update.
//
//   RAM sync: after each partial, 0x26 (old RAM) is updated to the current
//   frame so the next partial's diff is against the correct baseline.

#include "epd.h"
#include "hal_pins.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "epd";
static spi_device_handle_t spi_dev = NULL;

// Partial waveform LUT for SSD1681 / GDEY0154D67.
// Bytes 0–152 → register 0x32 (waveform table).
// Bytes 153–158 → border, gate voltage, source voltages, VCOM timing.
// Source: Waveshare epd1in54_V2 (same SSD1681 controller), confirmed working.
static const uint8_t LUT_PARTIAL[159] = {
    // VS waveform table (5 rows × 12 groups = 60 bytes)
    0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // LL: stay black
    0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // LH: black→white
    0x40,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // HL: white→black
    0x00,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // HH: stay white
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, // VCOM
    // Timing table (12 groups × 7 bytes = 84 bytes)
    // SR0 = drive cycle count for T0. Lower = faster partial but less contrast.
    // 0x0F (15) ≈ 300ms  |  0x08 (8) ≈ 160ms  |  0x04 (4) ≈ 80ms
    0x08,0x00,0x00,0x00,0x00,0x00,0x00,
    0x01,0x01,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    // GS transition sequence (9 bytes)
    0x22,0x22,0x22,0x22,0x22,0x22,0x00,0x00,0x00,
    // Voltage/timing for other registers (NOT written to 0x32)
    0x02, // [153] → 0x3F border waveform
    0x17, // [154] → 0x03 gate driving voltage
    0x41,0xB0,0x32, // [155–157] → 0x04 source driving voltages
    0x28, // [158] → 0x2C VCOM timing
};

/* ---- Low-level SPI ---- */

static void spi_tx(const uint8_t *data, size_t len) {
    spi_transaction_t txn = { .length = len * 8, .tx_buffer = data };
    spi_device_transmit(spi_dev, &txn);
}

static void cmd(uint8_t c) {
    gpio_set_level(PIN_EPD_CS, 0);
    gpio_set_level(PIN_EPD_DC, 0);
    spi_tx(&c, 1);
    gpio_set_level(PIN_EPD_CS, 1);
}

static void cmd1(uint8_t c, uint8_t d) {
    gpio_set_level(PIN_EPD_CS, 0);
    gpio_set_level(PIN_EPD_DC, 0);
    spi_tx(&c, 1);
    gpio_set_level(PIN_EPD_DC, 1);
    spi_tx(&d, 1);
    gpio_set_level(PIN_EPD_CS, 1);
}

static void cmd_buf(uint8_t c, const uint8_t *d, size_t len) {
    gpio_set_level(PIN_EPD_CS, 0);
    gpio_set_level(PIN_EPD_DC, 0);
    spi_tx(&c, 1);
    gpio_set_level(PIN_EPD_DC, 1);
    spi_tx(d, len);
    gpio_set_level(PIN_EPD_CS, 1);
}

static void data_bulk(const uint8_t *d, size_t len) {
    gpio_set_level(PIN_EPD_CS, 0);
    gpio_set_level(PIN_EPD_DC, 1);
    spi_tx(d, len);
    gpio_set_level(PIN_EPD_CS, 1);
}

/* ---- Busy / Reset ---- */

void epd_wait_busy(void) {
    int timeout = 500;
    while (gpio_get_level(PIN_EPD_BUSY) == 1 && timeout-- > 0)
        vTaskDelay(pdMS_TO_TICKS(10));
    if (timeout <= 0) ESP_LOGW(TAG, "BUSY timeout!");
}

bool epd_is_busy(void) {
    return gpio_get_level(PIN_EPD_BUSY) == 1;
}

static void hw_reset(void) {
    gpio_set_level(PIN_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_EPD_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_EPD_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    epd_wait_busy();
}

/* ---- RAM addressing ---- */

static void set_ram_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    cmd1(0x11, 0x03);
    uint8_t rx[] = { (uint8_t)(x / 8), (uint8_t)((x + w - 1) / 8) };
    cmd_buf(0x44, rx, 2);
    uint8_t ry[] = { y & 0xFF, y >> 8, (y + h - 1) & 0xFF, (y + h - 1) >> 8 };
    cmd_buf(0x45, ry, 4);
    cmd1(0x4E, x / 8);
    uint8_t cy[] = { y & 0xFF, y >> 8 };
    cmd_buf(0x4F, cy, 2);
}

static void write_full(uint8_t ram_cmd, const uint8_t *fb) {
    set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);
    cmd(ram_cmd);
    data_bulk(fb, EPD_FB_SIZE);
}

/* ---- Init (identical to original working driver, runs once at boot) ---- */

void epd_init(void) {
    gpio_config_t out = {
        .pin_bit_mask = (1ULL << PIN_EPD_CS) | (1ULL << PIN_EPD_DC) | (1ULL << PIN_EPD_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&out);
    gpio_config_t in = {
        .pin_bit_mask = (1ULL << PIN_EPD_BUSY),
        .mode = GPIO_MODE_INPUT,
    };
    gpio_config(&in);
    gpio_set_level(PIN_EPD_CS, 1);
    gpio_set_level(PIN_EPD_RST, 1);

    spi_bus_config_t bus = {
        .mosi_io_num = PIN_SPI_MOSI,
        .sclk_io_num = PIN_SPI_CLK,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EPD_FB_SIZE + 64,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(EPD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    spi_device_interface_config_t dev = {
        .clock_speed_hz = 4000000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(EPD_SPI_HOST, &dev, &spi_dev));

    hw_reset();
    vTaskDelay(pdMS_TO_TICKS(10));
    cmd(0x12);  // SW reset
    vTaskDelay(pdMS_TO_TICKS(10));
    epd_wait_busy();

    uint8_t drv[] = { 0xC7, 0x00, 0x00 };
    cmd_buf(0x01, drv, 3);       // driver output: 200 lines
    cmd1(0x3C, 0x05);            // border waveform
    cmd1(0x18, 0x80);            // internal temp sensor
    set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);

    ESP_LOGI(TAG, "EPD init OK (GDEY0154D67)");
}

/* ---- Full refresh ----
 *
 * Basemap sync: write fb to 0x24, then 0x26, then 0xF7 (Mode 1) full refresh.
 * Both RAMs end up with identical content — clean baseline for partial diffs.
 * No controller re-init (that caused an X-flip on this panel/FPC).
 */
void epd_refresh_full(const uint8_t *fb) {
    write_full(0x24, fb);
    write_full(0x26, fb);
    cmd1(0x22, 0xF7);
    cmd(0x20);
    epd_wait_busy();
}

/* ---- Windowed partial refresh ----
 *
 *   1. Load custom LUT (0x32) + voltage registers.
 *      Must happen before writing image data. No HW reset — a reset clears
 *      the loaded LUT and forces fallback to OTP (full-flash) on next update.
 *
 *   2. Set display option and border for partial mode.
 *
 *   3. Set RAM window and write only the changed region to 0x24 (BW RAM).
 *      0x26 (old RAM) still holds the previous full frame from the last sync,
 *      so the LUT sees the correct old→new diff for each pixel.
 *
 *   4. Trigger: 0x22 = 0xC7 (same phase sequence as 0xF7 full refresh, but
 *      skips the OTP LUT load step → uses our pre-loaded custom LUT).
 *
 *   5. RAM sync: write full fb to both 0x26 and 0x24.
 *      Resets the diff baseline for the next partial so it sees only
 *      incremental changes, not cumulative ones from the original basemap.
 */
static void do_partial(const uint8_t *fb,
                       uint16_t gfx_x, uint16_t gfx_y,
                       uint16_t gfx_w, uint16_t gfx_h) {

    // Step 1: Load partial waveform LUT and associated voltage registers
    cmd_buf(0x32, LUT_PARTIAL, 153);
    cmd1(0x3F, LUT_PARTIAL[153]); // border waveform
    cmd1(0x03, LUT_PARTIAL[154]); // gate driving voltage
    uint8_t src[3] = { LUT_PARTIAL[155], LUT_PARTIAL[156], LUT_PARTIAL[157] };
    cmd_buf(0x04, src, 3);        // source driving voltages
    cmd1(0x2C, LUT_PARTIAL[158]); // VCOM timing

    // Step 2: Display option (partial mode) and border
    uint8_t opt[] = { 0x00,0x00,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00 };
    cmd_buf(0x37, opt, 10);
    cmd1(0x3C, 0x80); // border: float

    // Step 3: Set RAM window and write changed region to 0x24
    uint8_t x_start = gfx_x / 8;
    uint8_t x_end   = (gfx_x + gfx_w - 1) / 8;
    uint16_t y_start = gfx_y;
    uint16_t y_end   = gfx_y + gfx_h - 1;

    uint8_t rx[] = { x_start, x_end };
    cmd_buf(0x44, rx, 2);
    uint8_t ry[] = { y_start & 0xFF, y_start >> 8, y_end & 0xFF, y_end >> 8 };
    cmd_buf(0x45, ry, 4);
    cmd1(0x4E, x_start);
    uint8_t cy[] = { y_start & 0xFF, y_start >> 8 };
    cmd_buf(0x4F, cy, 2);

    uint8_t bytes_per_row = x_end - x_start + 1;
    cmd(0x24);
    for (uint16_t row = y_start; row <= y_end; row++) {
        const uint8_t *row_ptr = fb + row * (EPD_WIDTH / 8) + x_start;
        data_bulk(row_ptr, bytes_per_row);
    }

    // Step 4: Trigger using loaded LUT (0xC7 = enable clock+analog, display, disable — no OTP load)
    cmd1(0x22, 0xC7);
    cmd(0x20);
    epd_wait_busy();

    // Step 5: Sync both RAMs to current frame for correct diff on next partial
    write_full(0x26, fb);
    write_full(0x24, fb);
}

/* ---- Deghost ---- */

// Full-white refresh to clear image retention, then partial refresh back to fb.
// The all-white pass drives every pixel to maximum white, erasing any ghosting.
// The subsequent partial uses the custom LUT to redraw content without a second
// full flash — resulting in one white blink then the frame reappears cleanly.
void epd_deghost(const uint8_t *fb) {
    // Write all-white (0xFF) to both RAMs using a small row buffer (avoids
    // putting EPD_FB_SIZE bytes on the stack)
    uint8_t white_row[EPD_WIDTH / 8];
    memset(white_row, 0xFF, sizeof(white_row));

    set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);
    cmd(0x24);
    for (int y = 0; y < EPD_HEIGHT; y++) data_bulk(white_row, sizeof(white_row));

    set_ram_area(0, 0, EPD_WIDTH, EPD_HEIGHT);
    cmd(0x26);
    for (int y = 0; y < EPD_HEIGHT; y++) data_bulk(white_row, sizeof(white_row));

    // Full OTP refresh → display goes all-white
    cmd1(0x22, 0xF7);
    cmd(0x20);
    epd_wait_busy();

    // Partial refresh back to current frame using the custom differential LUT.
    // Both RAMs hold all-white, so the LUT correctly identifies every non-white
    // pixel in fb as an HL transition and drives it to black — no full flash.
    do_partial(fb, 0, 0, EPD_WIDTH, EPD_HEIGHT);
}

/* ---- Public API ---- */

void epd_refresh_partial(const uint8_t *old_fb, const uint8_t *new_fb) {
    (void)old_fb;
    do_partial(new_fb, 0, 0, EPD_WIDTH, EPD_HEIGHT);
}

void epd_refresh_window(const uint8_t *old_fb, const uint8_t *new_fb, epd_rect_t rect) {
    (void)old_fb;
    if (rect.w == 0 || rect.h == 0) return;

    // Byte-align X to 8-pixel boundary
    uint16_t x0 = (rect.x / 8) * 8;
    uint16_t w  = rect.w + (rect.x - x0);
    if (w % 8) w += 8 - (w % 8);

    do_partial(new_fb, x0, rect.y, w, rect.h);
}

epd_refresh_t epd_refresh_smart(const uint8_t *old_fb, const uint8_t *new_fb,
                                 uint8_t *partial_count, bool force_full) {
    if (force_full) {
        epd_refresh_full(new_fb);
        *partial_count = 0;
        return EPD_REFRESH_FULL;
    }
    epd_rect_t diff = epd_diff_rect(old_fb, new_fb);
    if (diff.w == 0 && diff.h == 0) return EPD_REFRESH_PARTIAL;
    epd_refresh_window(old_fb, new_fb, diff);
    (*partial_count)++;
    return EPD_REFRESH_PARTIAL;
}

epd_refresh_t epd_refresh_animate(const uint8_t *old_fb, const uint8_t *new_fb,
                                   uint8_t *partial_count) {
    epd_rect_t diff = epd_diff_rect(old_fb, new_fb);
    if (diff.w == 0 && diff.h == 0) return EPD_REFRESH_PARTIAL;
    epd_refresh_window(old_fb, new_fb, diff);
    (*partial_count)++;
    return EPD_REFRESH_PARTIAL;
}

/* ---- Diff (unchanged) ---- */

epd_rect_t epd_diff_rect(const uint8_t *a, const uint8_t *b) {
    uint16_t min_x = EPD_WIDTH, max_x = 0, min_y = EPD_HEIGHT, max_y = 0;
    bool changed = false;
    for (uint16_t y = 0; y < EPD_HEIGHT; y++) {
        for (uint16_t xb = 0; xb < EPD_WIDTH / 8; xb++) {
            uint16_t idx = y * (EPD_WIDTH / 8) + xb;
            if (a[idx] != b[idx]) {
                changed = true;
                uint16_t px = xb * 8;
                if (px < min_x) min_x = px;
                if (px + 8 > max_x) max_x = px + 8;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
        }
    }
    if (!changed) return (epd_rect_t){0, 0, 0, 0};
    return (epd_rect_t){min_x, min_y, max_x - min_x, max_y - min_y + 1};
}

void epd_sleep(void) {
    cmd1(0x10, 0x01);
}
