#include "hal.h"
#include "hal_pins.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

static const char *TAG = "pwr";
static adc_oneshot_unit_handle_t adc_handle = NULL;
static adc_cali_handle_t cali_handle = NULL;

#define VDIV_RATIO      2.0f     // matches original: voltageDividerRatio = 2.0
#define BATT_MIN_MV     3200     // matches original: map(batt, 3200, ...)
#define BATT_MAX_MV     4200     // matches original: map(batt, ..., 4200, ...)
#define BATT_CRIT_V     3.2f
#define ADC_SAMPLES     16       // matches original: 16 sample average
#define BATT_ADC_UNIT   ADC_UNIT_1

void hal_power_init(void) {
    // Init ADC unit
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = BATT_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    // Configure channel
    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, BATT_ADC_CHAN, &chan_cfg));

    // Calibration for accurate millivolt readings (like analogReadMilliVolts)
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = BATT_ADC_UNIT,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t err = adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "ADC cal failed (%s), using raw", esp_err_to_name(err));
        cali_handle = NULL;
    }
}

// Returns voltage in volts, same as original: voltageDividerRatio * avg_mV / 1000
float hal_power_battery_voltage(void) {
    int total_mv = 0;
    for (int i = 0; i < ADC_SAMPLES; i++) {
        int raw;
        adc_oneshot_read(adc_handle, BATT_ADC_CHAN, &raw);
        if (cali_handle) {
            int mv;
            adc_cali_raw_to_voltage(cali_handle, raw, &mv);
            total_mv += mv;
        } else {
            total_mv += (raw * 3300) / 4095;
        }
    }
    // Original: voltageDividerRatio * Vbatt / 16 / 1000.0
    int avg_mv = total_mv / ADC_SAMPLES;
    float voltage = VDIV_RATIO * avg_mv / 1000.0f;
    return voltage;
}

// Original: battPercent = map(batt, 3200, 4200, 0, 100)
int hal_power_battery_percent(void) {
    float v = hal_power_battery_voltage();
    int batt_mv = (int)(v * 1000.0f);
    if (batt_mv <= BATT_MIN_MV) return 0;
    if (batt_mv >= BATT_MAX_MV) return 100;
    return (batt_mv - BATT_MIN_MV) * 100 / (BATT_MAX_MV - BATT_MIN_MV);
}

bool hal_power_is_critical(void) {
    return hal_power_battery_voltage() < BATT_CRIT_V;
}
