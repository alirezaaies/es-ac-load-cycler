/** @file hih.c */
#include "hih.h"

#include <stddef.h>

HIH_Config HIH_LegacyBoardConfig(void)
{
    const HIH_Config config = {
        .adc_full_scale = 4095U,
        .adc_reference_v = 3.336f,
        .sensor_offset_v = 0.853f,
        .sensor_slope_v_per_percent = 0.03f,
        .correction_gain = 1.3f * 1.04f
    };
    return config;
}

bool HIH_Convert(const HIH_Config *config,
                 uint32_t adc_code,
                 float *humidity_percent)
{
    float sensor_voltage;
    float humidity;

    if ((config == NULL) || (humidity_percent == NULL) ||
        (config->adc_full_scale == 0U) ||
        (config->adc_reference_v <= 0.0f) ||
        (config->sensor_slope_v_per_percent <= 0.0f) ||
        (config->correction_gain <= 0.0f)) {
        return false;
    }
    if (adc_code > config->adc_full_scale) {
        adc_code = config->adc_full_scale;
    }

    sensor_voltage = config->adc_reference_v *
                     ((float)adc_code / (float)config->adc_full_scale);
    humidity = ((sensor_voltage - config->sensor_offset_v) /
                config->sensor_slope_v_per_percent) * config->correction_gain;
    if (humidity < 0.0f) humidity = 0.0f;
    if (humidity > 100.0f) humidity = 100.0f;
    *humidity_percent = humidity;
    return true;
}
