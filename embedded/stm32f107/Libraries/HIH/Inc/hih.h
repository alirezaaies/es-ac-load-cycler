/** @file hih.h @brief Pure conversion helper for analog HIH humidity sensors. */
#ifndef HIH_H
#define HIH_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t adc_full_scale;
    float adc_reference_v;
    float sensor_offset_v;
    float sensor_slope_v_per_percent;
    float correction_gain;
} HIH_Config;

/** Fill the exact empirical constants used by the legacy test-room firmware. */
HIH_Config HIH_LegacyBoardConfig(void);

/**
 * @brief Convert one averaged ADC code to relative humidity and clamp 0..100%.
 * @return false for an invalid configuration or null output pointer.
 */
bool HIH_Convert(const HIH_Config *config,
                 uint32_t adc_code,
                 float *humidity_percent);

#endif /* HIH_H */
