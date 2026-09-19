/**
 * @file sensor_address_store.h
 * @brief Versioned STM32F107 Flash storage for 32 DS18B20 ROM identifiers.
 *
 * This project service is dormant in the LED-only baseline. The linker still
 * reserves the final 2 KiB page so previously stored addresses are preserved.
 */
#ifndef SENSOR_ADDRESS_STORE_H
#define SENSOR_ADDRESS_STORE_H

#include <stdbool.h>
#include <stdint.h>

#define SENSOR_ADDRESS_STORE_COUNT 32U
#define SENSOR_ADDRESS_STORE_ROM_SIZE 8U

/** Load and validate the current record or migrate one valid legacy table. */
bool SensorAddressStore_Load(
    uint8_t addresses[SENSOR_ADDRESS_STORE_COUNT][SENSOR_ADDRESS_STORE_ROM_SIZE]);

/** Validate and atomically replace the single-page record as far as HAL allows. */
bool SensorAddressStore_Save(
    const uint8_t addresses[SENSOR_ADDRESS_STORE_COUNT][SENSOR_ADDRESS_STORE_ROM_SIZE]);

/** Validate family 0x28 and the Dallas/Maxim CRC byte of one ROM. */
bool SensorAddressStore_IsValidRom(
    const uint8_t address[SENSOR_ADDRESS_STORE_ROM_SIZE]);

#endif /* SENSOR_ADDRESS_STORE_H */
