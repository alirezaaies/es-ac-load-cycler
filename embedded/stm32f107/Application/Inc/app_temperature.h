/**
 * @file app_temperature.h
 * @brief Simple automatic temperature interface for application code.
 *
 * Normal use requires no discovery or assignment calls. Connect sensors and
 * call App_Process() from the main loop; new ROMs take the first free numbers.
 */
#ifndef APP_TEMPERATURE_H
#define APP_TEMPERATURE_H

#include <stdbool.h>
#include <stdint.h>

#define APP_TEMPERATURE_SENSOR_COUNT 32U

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Latest Celsius values for the numbered sensors.
 *
 * Index 0 is sensor 1. Read an element only when the matching validity flag
 * is true. Invalid elements are set to 0.0F to avoid exposing stale data.
 */
extern volatile float g_temperature_c[APP_TEMPERATURE_SENSOR_COUNT];

/** @brief Validity flags corresponding one-to-one with g_temperature_c. */
extern volatile bool g_temperature_valid[APP_TEMPERATURE_SENSOR_COUNT];

/**
 * @brief Number of persistent sensor identities, including absent sensors.
 */
extern volatile uint8_t g_temperature_sensor_count;

/**
 * @brief Get one numbered temperature with validity checking.
 * @param sensor_number Human-facing number starting at 1.
 * @param temperature_c Receives degrees Celsius when a valid sample exists.
 * @return true when the output was updated, otherwise false.
 */
bool App_TemperatureGetCelsius(uint8_t sensor_number, float *temperature_c);

#ifdef __cplusplus
}
#endif

#endif /* APP_TEMPERATURE_H */
