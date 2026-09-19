/**
 * @file ds18b20_manager.h
 * @brief Portable multi-bus DS18B20 discovery, stable numbering, and sampling.
 *
 * Logical sensor number is persistent and independent of Search ROM order.
 * Human-facing number 1 maps to slots[0], number 2 to slots[1], and so on.
 */
#ifndef DS18B20_MANAGER_H
#define DS18B20_MANAGER_H

#include "ds18b20.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DS18B20_MANAGER_MAX_SENSORS
#define DS18B20_MANAGER_MAX_SENSORS 32U
#endif

#ifndef DS18B20_MANAGER_MAX_BUSES
#define DS18B20_MANAGER_MAX_BUSES 4U
#endif

#define DS18B20_MANAGER_UNASSIGNED_SLOT 0xFFU

typedef enum {
    DS18B20_MANAGER_STATUS_OK = 0,
    DS18B20_MANAGER_STATUS_INVALID_ARGUMENT,
    DS18B20_MANAGER_STATUS_BUSY,
    DS18B20_MANAGER_STATUS_NOT_FOUND,
    DS18B20_MANAGER_STATUS_DUPLICATE,
    DS18B20_MANAGER_STATUS_STORAGE_ERROR,
    DS18B20_MANAGER_STATUS_CAPACITY_EXCEEDED
} DS18B20_ManagerStatus;

typedef enum {
    DS18B20_MANAGER_IDLE = 0,
    DS18B20_MANAGER_CONVERTING,
    DS18B20_MANAGER_READING
} DS18B20_ManagerPhase;

/** Persistent part of one numbered sensor slot. */
typedef struct {
    uint8_t assigned;
    uint8_t bus_index;
    uint8_t rom[DS18B20_ROM_SIZE];
} DS18B20_MappingEntry;

/** One ROM found during the latest discovery pass. */
typedef struct {
    uint8_t bus_index;
    uint8_t rom[DS18B20_ROM_SIZE];
    uint8_t assigned_slot; /**< Zero-based slot or UNASSIGNED_SLOT. */
} DS18B20_DiscoveredDevice;

/** Persistent identity plus live values for one logical sensor number. */
typedef struct {
    DS18B20_MappingEntry mapping;
    bool present;
    bool valid;
    int16_t raw_temperature;
    float temperature_c;
    DS18B20_Resolution resolution;
    DS18B20_Status last_status;
    uint32_t updated_at_ms;
    uint32_t successful_reads;
    uint32_t failed_reads;
} DS18B20_SensorSlot;

typedef uint32_t (*DS18B20_ManagerGetTimeMs)(void *context);
typedef bool (*DS18B20_ManagerLoadMappings)(
    void *context, DS18B20_MappingEntry *entries, uint8_t entry_count);
typedef bool (*DS18B20_ManagerSaveMappings)(
    void *context, const DS18B20_MappingEntry *entries, uint8_t entry_count);

typedef struct {
    OneWire_Bus *buses;
    uint8_t bus_count;
    uint8_t sensor_count;
    uint32_t sample_interval_ms;
    uint32_t discovery_interval_ms;
    uint16_t conversion_time_ms;
    bool auto_replace_unambiguous;
    void *time_context;
    DS18B20_ManagerGetTimeMs get_time_ms;
    void *storage_context;
    DS18B20_ManagerLoadMappings load_mappings;
    DS18B20_ManagerSaveMappings save_mappings;
} DS18B20_ManagerConfig;

/** Complete manager state; intentionally debugger-friendly and allocation-free. */
typedef struct {
    DS18B20_ManagerConfig config;
    DS18B20_SensorSlot slots[DS18B20_MANAGER_MAX_SENSORS];
    DS18B20_MappingEntry persistence_buffer[DS18B20_MANAGER_MAX_SENSORS];
    DS18B20_DiscoveredDevice discovered[DS18B20_MANAGER_MAX_SENSORS];
    uint8_t discovered_count;
    bool bus_online[DS18B20_MANAGER_MAX_BUSES];
    bool mapping_loaded;
    bool discovery_overflow;
    DS18B20_ManagerPhase phase;
    DS18B20_ManagerStatus last_status;
    uint8_t read_index;
    uint32_t conversion_deadline_ms;
    uint32_t next_sample_ms;
    uint32_t next_discovery_ms;
    uint32_t discovery_passes;
    uint32_t automatic_replacements;
} DS18B20_Manager;

/** @brief Initialize and load mappings. @param manager Destination. @param configuration Buses, timing, and callbacks. @return Status. */
DS18B20_ManagerStatus DS18B20_ManagerInit(
    DS18B20_Manager *manager, const DS18B20_ManagerConfig *configuration);

/** @brief Discover while idle. @param manager Initialized instance. @return Discovery/busy/storage status. */
DS18B20_ManagerStatus DS18B20_ManagerDiscover(DS18B20_Manager *manager);

/**
 * Assign a discovered ROM to a human-facing logical number (1..sensor_count).
 * The same call performs an explicit replacement when that number was assigned.
 * @param manager Initialized instance.
 * @param logical_number Human-facing number.
 * @param discovery_index Index in the latest discovered array.
 * @return Assignment status.
 */
DS18B20_ManagerStatus DS18B20_ManagerAssignDiscovered(
    DS18B20_Manager *manager, uint8_t logical_number, uint8_t discovery_index);

/** @brief Assign a known identity. @param manager Instance. @param logical_number Human number. @param bus_index Bus. @param rom Valid ROM. @return Status. */
DS18B20_ManagerStatus DS18B20_ManagerAssignRom(
    DS18B20_Manager *manager, uint8_t logical_number, uint8_t bus_index,
    const uint8_t rom[DS18B20_ROM_SIZE]);

/** @brief Clear one mapping. @param manager Instance. @param logical_number Human number. @return Status. */
DS18B20_ManagerStatus DS18B20_ManagerClear(
    DS18B20_Manager *manager, uint8_t logical_number);

/** @brief Service discovery, conversion, and one read. @param manager Instance. */
void DS18B20_ManagerProcess(DS18B20_Manager *manager);

#ifdef __cplusplus
}
#endif

#endif /* DS18B20_MANAGER_H */
