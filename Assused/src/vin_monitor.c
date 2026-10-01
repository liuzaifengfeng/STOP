#include "vin_monitor.h"

#if VIN_MONITOR_ENABLED
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cal;

esp_err_t vin_monitor_init(void)
{
    adc_oneshot_unit_init_cfg_t unit = {.unit_id = ADC_UNIT_1};
    esp_err_t err = adc_oneshot_new_unit(&unit, &s_adc);
    if (err != ESP_OK) return err;
    adc_oneshot_chan_cfg_t channel = {
        .atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_12,
    };
    err = adc_oneshot_config_channel(s_adc, ADC_CHANNEL_0, &channel);
    if (err != ESP_OK) return err;
    adc_cali_curve_fitting_config_t calibration = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    return adc_cali_create_scheme_curve_fitting(&calibration, &s_cal);
}

esp_err_t vin_monitor_read_mv(int32_t *vin_mv)
{
    if (!vin_mv || !s_adc || !s_cal) return ESP_ERR_INVALID_STATE;
    int32_t sum = 0;
    for (unsigned i = 0; i < 8; ++i) {
        int raw, adc_mv;
        esp_err_t err = adc_oneshot_read(s_adc, ADC_CHANNEL_0, &raw);
        if (err != ESP_OK) return err;
        err = adc_cali_raw_to_voltage(s_cal, raw, &adc_mv);
        if (err != ESP_OK) return err;
        sum += adc_mv;
    }
    *vin_mv = (sum / 8) * 11;
    return ESP_OK;
}

#else
esp_err_t vin_monitor_init(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t vin_monitor_read_mv(int32_t *vin_mv)
{
    if (vin_mv) *vin_mv = 0;
    return ESP_ERR_NOT_SUPPORTED;
}
#endif
