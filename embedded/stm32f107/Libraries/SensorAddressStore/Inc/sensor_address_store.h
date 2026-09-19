/**
 * @file sensor_address_store.h
 * @brief CRC-protected STM32F107 Flash blob for persistent sensor mappings.
 *
 * This is the only MCU-specific persistence layer. The portable DS18B20
 * manager receives it through load/save callbacks and never includes HAL.
 */
#ifndef SENSOR_ADDRESS_STORE_H
#define SENSOR_ADDRESS_STORE_H

#include <stdbool.h>
#include <stdint.h>

#define SENSOR_ADDRESS_STORE_MAX_PAYLOAD 512U
#define SENSOR_ADDRESS_STORE_LEGACY_COUNT 32U
#define SENSOR_ADDRESS_STORE_ROM_SIZE 8U

/** @brief Load a versioned record. @param payload Output buffer. @param payload_size Exact expected size. @return true when header and CRC are valid. */
bool SensorAddressStore_Load(void *payload, uint16_t payload_size);

/** @brief Save a CRC-protected blob. @param payload Input bytes. @param payload_size Byte count. @return true after write verification. */
bool SensorAddressStore_Save(const void *payload, uint16_t payload_size);

/**
 * Load the former 32-ROM format for one-time migration.
 * Returns false unless all legacy entries have a valid family and ROM CRC.
 * @param addresses Output array of 32 ROM codes.
 * @return true only for a complete valid legacy table.
 */
bool SensorAddressStore_LoadLegacy(
    uint8_t addresses[SENSOR_ADDRESS_STORE_LEGACY_COUNT]
                     [SENSOR_ADDRESS_STORE_ROM_SIZE]);

#endif /* SENSOR_ADDRESS_STORE_H */
