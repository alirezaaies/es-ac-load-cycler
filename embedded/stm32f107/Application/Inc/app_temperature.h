/**
 * @file app_temperature.h
 * @brief Simple automatic temperature interface for application code.
 *
 * Normal use requires no discovery or assignment calls. Connect sensors and
 * call App_Process() from the main loop; new ROMs take the first free numbers.
 */
#ifndef APP_TEMPERATURE_H
#define APP_TEMPERATURE_H

#include "ds18b20_manager.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_TEMPERATURE_SENSOR_COUNT 32U

#ifdef __cplusplus
extern "C" {
#endif

/** Celsius values; index 0 is sensor 1, index 1 is sensor 2, and so on. */
extern volatile float g_temperature_c[APP_TEMPERATURE_SENSOR_COUNT];

/** A value may be used only when the matching element is true. */
extern volatile bool g_temperature_valid[APP_TEMPERATURE_SENSOR_COUNT];

/** Number of sensor identities registered automatically in persistent slots. */
extern volatile uint8_t g_temperature_sensor_count;

/**
 * @brief Get one numbered temperature with validity checking.
 * @param sensor_number Human-facing number starting at 1.
 * @param temperature_c Receives degrees Celsius when a valid sample exists.
 * @return true when the output was updated, otherwise false.
 */
bool App_TemperatureGetCelsius(uint8_t sensor_number, float *temperature_c);

/* The items below are for diagnostics or production tools, not normal use. */
extern DS18B20_Manager g_app_temperature;
DS18B20_ManagerStatus App_TemperatureDiscover(void);
DS18B20_ManagerStatus App_TemperatureAssignDiscovered(
    uint8_t logical_number, uint8_t discovery_index);
DS18B20_ManagerStatus App_TemperatureAssignRom(
    uint8_t logical_number, uint8_t bus_index,
    const uint8_t rom[DS18B20_ROM_SIZE]);
DS18B20_ManagerStatus App_TemperatureClear(uint8_t logical_number);

#ifdef __cplusplus
}
#endif

#endif /* APP_TEMPERATURE_H */
