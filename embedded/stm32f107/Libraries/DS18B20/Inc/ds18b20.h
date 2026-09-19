/**
 * @file ds18b20.h
 * @brief Portable addressed DS18B20 operations over a OneWire_Bus.
 */
#ifndef DS18B20_H
#define DS18B20_H

#include "one_wire.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS18B20_ROM_SIZE ONE_WIRE_ROM_SIZE
#define DS18B20_SCRATCHPAD_SIZE 9U
#define DS18B20_FAMILY_CODE 0x28U

typedef enum {
    DS18B20_STATUS_OK = 0,
    DS18B20_STATUS_INVALID_ARGUMENT,
    DS18B20_STATUS_NO_DEVICE,
    DS18B20_STATUS_TIMEOUT,
    DS18B20_STATUS_CRC_ERROR,
    DS18B20_STATUS_OUT_OF_RANGE
} DS18B20_Status;

typedef enum {
    DS18B20_RESOLUTION_9_BIT = 9,
    DS18B20_RESOLUTION_10_BIT = 10,
    DS18B20_RESOLUTION_11_BIT = 11,
    DS18B20_RESOLUTION_12_BIT = 12
} DS18B20_Resolution;

/** @brief Validate family and CRC. @param rom Eight-byte ROM. @return true when valid. */
bool DS18B20_IsValidRom(const uint8_t rom[DS18B20_ROM_SIZE]);

/** @brief Read the sole ROM. @param bus Target bus. @param rom Output. @return Operation status. */
DS18B20_Status DS18B20_ReadRom(const OneWire_Bus *bus,
                               uint8_t rom[DS18B20_ROM_SIZE]);

/** @brief Start conversion. @param bus Target. @param rom ROM or NULL for broadcast. @return Status. */
DS18B20_Status DS18B20_StartConversion(const OneWire_Bus *bus,
                                       const uint8_t rom[DS18B20_ROM_SIZE]);

/** @brief Poll conversion. @param bus Target. @return true when complete. */
bool DS18B20_IsConversionReady(const OneWire_Bus *bus);

/**
 * @brief Read and verify a scratchpad.
 * @param bus Target bus.
 * @param rom Sensor ROM, or NULL when exactly one sensor is on the bus.
 * @param scratchpad Nine-byte output, updated before CRC status is returned.
 * @return Operation status.
 */
DS18B20_Status DS18B20_ReadScratchpad(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE]);

/** @brief Set volatile alarms/resolution. @param bus Target. @param rom ROM. @param alarm_high_c TH. @param alarm_low_c TL. @param resolution Resolution. @return Status. */
DS18B20_Status DS18B20_WriteScratchpad(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    int8_t alarm_high_c,
    int8_t alarm_low_c,
    DS18B20_Resolution resolution);

/** @brief Decode temperature. @param scratchpad Input. @return Signed 1/16 degree C. */
int16_t DS18B20_DecodeRaw(
    const uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE]);

/** @brief Convert raw temperature. @param raw_temperature 1/16 degree value. @return Degrees Celsius. */
float DS18B20_RawToCelsius(int16_t raw_temperature);

/** @brief Decode resolution. @param scratchpad Input. @return Resolution. */
DS18B20_Resolution DS18B20_GetResolution(
    const uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE]);

/** @brief Get maximum conversion time. @param resolution Resolution. @return Milliseconds. */
uint16_t DS18B20_GetConversionTimeMs(DS18B20_Resolution resolution);

/**
 * @brief Portable blocking convenience read.
 * @param rom Sensor ROM, or NULL when exactly one sensor is on the bus.
 * @param timeout_ms Maximum wait; use at least 750 ms for unknown resolution.
 * @param temperature_c Updated only after a valid scratchpad CRC.
 * @return Operation status.
 *
 * Production code should normally use the nonblocking manager instead.
 */
DS18B20_Status DS18B20_ReadTemperatureBlocking(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    uint32_t timeout_ms,
    float *temperature_c);

#ifdef __cplusplus
}
#endif

#endif /* DS18B20_H */
