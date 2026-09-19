/**
 * @file ds18b20.h
 * @brief Addressed DS18B20 temperature sensor operations over OneWire_Bus.
 */
#ifndef DS18B20_H
#define DS18B20_H

#include "one_wire.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DS18B20_ROM_SIZE 8U
#define DS18B20_SCRATCHPAD_SIZE 9U
#define DS18B20_FAMILY_CODE 0x28U

typedef enum {
    DS18B20_STATUS_OK = 0,
    DS18B20_STATUS_INVALID_ARGUMENT,
    DS18B20_STATUS_NO_DEVICE,
    DS18B20_STATUS_TIMEOUT,
    DS18B20_STATUS_CRC_ERROR
} DS18B20_Status;

/** Validate family byte and ROM CRC. */
bool DS18B20_IsValidRom(const uint8_t rom[DS18B20_ROM_SIZE]);

/** Read the ROM code when exactly one device is connected to the bus. */
DS18B20_Status DS18B20_ReadRom(const OneWire_Bus *bus,
                               uint8_t rom[DS18B20_ROM_SIZE]);

/** Start conversion for one ROM, or every device when rom is NULL. */
DS18B20_Status DS18B20_StartConversion(const OneWire_Bus *bus,
                                       const uint8_t rom[DS18B20_ROM_SIZE]);

/** Return true after a powered sensor reports that conversion has completed. */
bool DS18B20_IsConversionReady(const OneWire_Bus *bus);

/** Select one sensor, read nine scratchpad bytes, and verify their CRC. */
DS18B20_Status DS18B20_ReadScratchpad(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE]);

/** Decode scratchpad bytes zero and one into signed raw units of 1/16 C. */
int16_t DS18B20_DecodeRaw(const uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE]);

/** Convert signed raw units to degrees Celsius. */
float DS18B20_RawToCelsius(int16_t raw_temperature);

/**
 * @brief Blocking convenience read with bounded conversion timeout.
 * @param timeout_ms Maximum conversion wait; 750 ms covers 12-bit resolution.
 * @param temperature_c Receives the decoded temperature only on success.
 */
DS18B20_Status DS18B20_ReadTemperature(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    uint32_t timeout_ms,
    float *temperature_c);

#ifdef __cplusplus
}
#endif

#endif /* DS18B20_H */
