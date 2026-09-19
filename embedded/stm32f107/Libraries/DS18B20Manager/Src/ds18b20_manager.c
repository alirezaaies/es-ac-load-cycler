/** @file ds18b20_manager.c */
#include "ds18b20_manager.h"

#include <string.h>

static bool deadline_reached(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static bool same_device(uint8_t bus_a, const uint8_t *rom_a,
                        uint8_t bus_b, const uint8_t *rom_b)
{
    return (bus_a == bus_b) &&
           (memcmp(rom_a, rom_b, DS18B20_ROM_SIZE) == 0);
}

static bool mapping_is_valid(const DS18B20_Manager *manager,
                             const DS18B20_MappingEntry *entry)
{
    return (entry->assigned == 1U) &&
           (entry->bus_index < manager->config.bus_count) &&
           DS18B20_IsValidRom(entry->rom);
}

static bool save_mappings(DS18B20_Manager *manager)
{
    if (manager->config.save_mappings == NULL) {
        return true;
    }
    for (uint8_t index = 0U; index < manager->config.sensor_count; ++index) {
        manager->persistence_buffer[index] = manager->slots[index].mapping;
    }
    return manager->config.save_mappings(manager->config.storage_context,
                                          manager->persistence_buffer,
                                          manager->config.sensor_count);
}

static void refresh_assignment_links(DS18B20_Manager *manager)
{
    for (uint8_t slot = 0U; slot < manager->config.sensor_count; ++slot) {
        manager->slots[slot].present = false;
    }
    for (uint8_t found = 0U; found < manager->discovered_count; ++found) {
        manager->discovered[found].assigned_slot =
            DS18B20_MANAGER_UNASSIGNED_SLOT;
        for (uint8_t slot = 0U; slot < manager->config.sensor_count; ++slot) {
            const DS18B20_MappingEntry *mapping =
                &manager->slots[slot].mapping;
            if (mapping_is_valid(manager, mapping) &&
                same_device(mapping->bus_index, mapping->rom,
                            manager->discovered[found].bus_index,
                            manager->discovered[found].rom)) {
                manager->discovered[found].assigned_slot = slot;
                manager->slots[slot].present = true;
                break;
            }
        }
    }
}

static void sort_discovered(DS18B20_Manager *manager)
{
    for (uint8_t i = 1U; i < manager->discovered_count; ++i) {
        DS18B20_DiscoveredDevice value = manager->discovered[i];
        uint8_t position = i;

        while (position > 0U) {
            const DS18B20_DiscoveredDevice *previous =
                &manager->discovered[position - 1U];
            const int rom_order = memcmp(previous->rom, value.rom,
                                         DS18B20_ROM_SIZE);
            if ((previous->bus_index < value.bus_index) ||
                ((previous->bus_index == value.bus_index) &&
                 (rom_order <= 0))) {
                break;
            }
            manager->discovered[position] = *previous;
            --position;
        }
        manager->discovered[position] = value;
    }
}

static DS18B20_ManagerStatus assign_rom(DS18B20_Manager *manager,
                                        uint8_t slot,
                                        uint8_t bus_index,
                                        const uint8_t *rom,
                                        bool count_replacement)
{
    DS18B20_SensorSlot previous;

    for (uint8_t other = 0U; other < manager->config.sensor_count; ++other) {
        const DS18B20_MappingEntry *candidate =
            &manager->slots[other].mapping;
        if ((other != slot) && mapping_is_valid(manager, candidate) &&
            same_device(candidate->bus_index, candidate->rom,
                        bus_index, rom)) {
            return DS18B20_MANAGER_STATUS_DUPLICATE;
        }
    }

    previous = manager->slots[slot];
    manager->slots[slot].mapping.assigned = 1U;
    manager->slots[slot].mapping.bus_index = bus_index;
    memcpy(manager->slots[slot].mapping.rom, rom, DS18B20_ROM_SIZE);
    manager->slots[slot].present = true;
    manager->slots[slot].valid = false;
    manager->slots[slot].last_status = DS18B20_STATUS_NO_DEVICE;

    if (!save_mappings(manager)) {
        manager->slots[slot] = previous;
        refresh_assignment_links(manager);
        return DS18B20_MANAGER_STATUS_STORAGE_ERROR;
    }
    refresh_assignment_links(manager);
    if (count_replacement && (previous.mapping.assigned == 1U)) {
        ++manager->automatic_replacements;
    }
    return DS18B20_MANAGER_STATUS_OK;
}

static DS18B20_ManagerStatus auto_replace(DS18B20_Manager *manager)
{
    if (!manager->config.auto_replace_unambiguous) {
        return DS18B20_MANAGER_STATUS_OK;
    }
    for (uint8_t bus = 0U; bus < manager->config.bus_count; ++bus) {
        uint8_t missing_count = 0U;
        uint8_t missing_slot = 0U;
        uint8_t new_count = 0U;
        uint8_t new_index = 0U;

        for (uint8_t slot = 0U; slot < manager->config.sensor_count; ++slot) {
            if (mapping_is_valid(manager, &manager->slots[slot].mapping) &&
                (manager->slots[slot].mapping.bus_index == bus) &&
                !manager->slots[slot].present) {
                ++missing_count;
                missing_slot = slot;
            }
        }
        for (uint8_t found = 0U; found < manager->discovered_count; ++found) {
            if ((manager->discovered[found].bus_index == bus) &&
                (manager->discovered[found].assigned_slot ==
                 DS18B20_MANAGER_UNASSIGNED_SLOT)) {
                ++new_count;
                new_index = found;
            }
        }
        if ((missing_count == 1U) && (new_count == 1U)) {
            const DS18B20_ManagerStatus status = assign_rom(
                manager, missing_slot, bus,
                manager->discovered[new_index].rom, true);
            if (status != DS18B20_MANAGER_STATUS_OK) {
                return status;
            }
        }
    }
    return DS18B20_MANAGER_STATUS_OK;
}

DS18B20_ManagerStatus DS18B20_ManagerInit(
    DS18B20_Manager *manager, const DS18B20_ManagerConfig *configuration)
{
    const uint32_t now = (configuration != NULL) &&
                         (configuration->get_time_ms != NULL)
                             ? configuration->get_time_ms(
                                   configuration->time_context)
                             : 0U;

    if ((manager == NULL) || (configuration == NULL) ||
        (configuration->buses == NULL) ||
        (configuration->bus_count == 0U) ||
        (configuration->bus_count > DS18B20_MANAGER_MAX_BUSES) ||
        (configuration->sensor_count == 0U) ||
        (configuration->sensor_count > DS18B20_MANAGER_MAX_SENSORS) ||
        (configuration->get_time_ms == NULL) ||
        (configuration->sample_interval_ms == 0U) ||
        (configuration->discovery_interval_ms == 0U) ||
        (configuration->conversion_time_ms == 0U)) {
        return DS18B20_MANAGER_STATUS_INVALID_ARGUMENT;
    }
    for (uint8_t bus = 0U; bus < configuration->bus_count; ++bus) {
        if (!OneWire_IsValid(&configuration->buses[bus])) {
            return DS18B20_MANAGER_STATUS_INVALID_ARGUMENT;
        }
    }

    memset(manager, 0, sizeof(*manager));
    manager->config = *configuration;
    manager->phase = DS18B20_MANAGER_IDLE;
    manager->next_sample_ms = now;
    manager->next_discovery_ms = now;
    manager->last_status = DS18B20_MANAGER_STATUS_OK;
    for (uint8_t slot = 0U; slot < manager->config.sensor_count; ++slot) {
        manager->slots[slot].last_status = DS18B20_STATUS_NO_DEVICE;
        manager->slots[slot].resolution = DS18B20_RESOLUTION_12_BIT;
    }

    memset(manager->persistence_buffer, 0,
           sizeof(manager->persistence_buffer));
    if ((manager->config.load_mappings != NULL) &&
        manager->config.load_mappings(manager->config.storage_context,
                                      manager->persistence_buffer,
                                      manager->config.sensor_count)) {
        manager->mapping_loaded = true;
        for (uint8_t slot = 0U; slot < manager->config.sensor_count; ++slot) {
            const DS18B20_MappingEntry *loaded =
                &manager->persistence_buffer[slot];
            bool duplicate = false;

            for (uint8_t previous = 0U; previous < slot; ++previous) {
                const DS18B20_MappingEntry *accepted =
                    &manager->slots[previous].mapping;
                if (mapping_is_valid(manager, accepted) &&
                    same_device(accepted->bus_index, accepted->rom,
                                loaded->bus_index, loaded->rom)) {
                    duplicate = true;
                    break;
                }
            }
            if ((loaded->assigned == 1U) &&
                (loaded->bus_index < manager->config.bus_count) &&
                DS18B20_IsValidRom(loaded->rom) && !duplicate) {
                manager->slots[slot].mapping = *loaded;
            }
        }
    }
    return DS18B20_MANAGER_STATUS_OK;
}

DS18B20_ManagerStatus DS18B20_ManagerDiscover(DS18B20_Manager *manager)
{
    if (manager == NULL) {
        return DS18B20_MANAGER_STATUS_INVALID_ARGUMENT;
    }
    if (manager->phase != DS18B20_MANAGER_IDLE) {
        return DS18B20_MANAGER_STATUS_BUSY;
    }

    manager->discovered_count = 0U;
    manager->discovery_overflow = false;
    memset(manager->bus_online, 0, sizeof(manager->bus_online));
    for (uint8_t bus = 0U; bus < manager->config.bus_count; ++bus) {
        OneWire_SearchState search;
        uint8_t rom[DS18B20_ROM_SIZE];
        bool found = OneWire_SearchFirst(&manager->config.buses[bus],
                                         &search, rom);

        while (found) {
            manager->bus_online[bus] = true;
            if (DS18B20_IsValidRom(rom)) {
                if (manager->discovered_count >=
                    DS18B20_MANAGER_MAX_SENSORS) {
                    manager->discovery_overflow = true;
                    break;
                }
                DS18B20_DiscoveredDevice *device =
                    &manager->discovered[manager->discovered_count++];
                device->bus_index = bus;
                memcpy(device->rom, rom, DS18B20_ROM_SIZE);
                device->assigned_slot = DS18B20_MANAGER_UNASSIGNED_SLOT;
            }
            found = OneWire_SearchNext(&manager->config.buses[bus],
                                       &search, rom);
        }
    }
    sort_discovered(manager);
    refresh_assignment_links(manager);
    ++manager->discovery_passes;
    manager->last_status = manager->discovery_overflow
                               ? DS18B20_MANAGER_STATUS_CAPACITY_EXCEEDED
                               : auto_replace(manager);
    return manager->last_status;
}

DS18B20_ManagerStatus DS18B20_ManagerAssignDiscovered(
    DS18B20_Manager *manager, uint8_t logical_number, uint8_t discovery_index)
{
    if ((manager == NULL) || (logical_number == 0U) ||
        (logical_number > manager->config.sensor_count) ||
        (discovery_index >= manager->discovered_count)) {
        return DS18B20_MANAGER_STATUS_INVALID_ARGUMENT;
    }
    manager->last_status = assign_rom(
        manager, (uint8_t)(logical_number - 1U),
        manager->discovered[discovery_index].bus_index,
        manager->discovered[discovery_index].rom, false);
    return manager->last_status;
}

DS18B20_ManagerStatus DS18B20_ManagerAssignRom(
    DS18B20_Manager *manager, uint8_t logical_number, uint8_t bus_index,
    const uint8_t rom[DS18B20_ROM_SIZE])
{
    if ((manager == NULL) || (logical_number == 0U) ||
        (logical_number > manager->config.sensor_count) ||
        (bus_index >= manager->config.bus_count) ||
        !DS18B20_IsValidRom(rom)) {
        return DS18B20_MANAGER_STATUS_INVALID_ARGUMENT;
    }
    manager->last_status = assign_rom(manager,
                                      (uint8_t)(logical_number - 1U),
                                      bus_index, rom, false);
    return manager->last_status;
}

DS18B20_ManagerStatus DS18B20_ManagerClear(
    DS18B20_Manager *manager, uint8_t logical_number)
{
    DS18B20_SensorSlot previous;
    uint8_t slot;

    if ((manager == NULL) || (logical_number == 0U) ||
        (logical_number > manager->config.sensor_count)) {
        return DS18B20_MANAGER_STATUS_INVALID_ARGUMENT;
    }
    slot = (uint8_t)(logical_number - 1U);
    previous = manager->slots[slot];
    memset(&manager->slots[slot], 0, sizeof(manager->slots[slot]));
    manager->slots[slot].last_status = DS18B20_STATUS_NO_DEVICE;
    manager->slots[slot].resolution = DS18B20_RESOLUTION_12_BIT;
    if (!save_mappings(manager)) {
        manager->slots[slot] = previous;
        manager->last_status = DS18B20_MANAGER_STATUS_STORAGE_ERROR;
        return manager->last_status;
    }
    refresh_assignment_links(manager);
    manager->last_status = DS18B20_MANAGER_STATUS_OK;
    return manager->last_status;
}

static void start_conversions(DS18B20_Manager *manager, uint32_t now)
{
    bool started = false;

    for (uint8_t bus = 0U; bus < manager->config.bus_count; ++bus) {
        if (DS18B20_StartConversion(&manager->config.buses[bus], NULL) ==
            DS18B20_STATUS_OK) {
            manager->bus_online[bus] = true;
            started = true;
        } else {
            manager->bus_online[bus] = false;
        }
    }
    if (started) {
        manager->phase = DS18B20_MANAGER_CONVERTING;
        manager->conversion_deadline_ms =
            now + manager->config.conversion_time_ms;
    } else {
        manager->next_sample_ms = now + manager->config.sample_interval_ms;
    }
}

static void read_one_slot(DS18B20_Manager *manager, uint32_t now)
{
    while (manager->read_index < manager->config.sensor_count) {
        DS18B20_SensorSlot *slot = &manager->slots[manager->read_index++];
        uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE];

        if (!mapping_is_valid(manager, &slot->mapping)) {
            continue;
        }
        if (!slot->present) {
            slot->valid = false;
            slot->last_status = DS18B20_STATUS_NO_DEVICE;
            ++slot->failed_reads;
            return;
        }

        slot->last_status = DS18B20_ReadScratchpad(
            &manager->config.buses[slot->mapping.bus_index],
            slot->mapping.rom, scratchpad);
        if (slot->last_status == DS18B20_STATUS_OK) {
            const int16_t raw = DS18B20_DecodeRaw(scratchpad);
            const float temperature = DS18B20_RawToCelsius(raw);
            if ((temperature >= -55.0F) && (temperature <= 125.0F)) {
                slot->raw_temperature = raw;
                slot->temperature_c = temperature;
                slot->resolution = DS18B20_GetResolution(scratchpad);
                slot->updated_at_ms = now;
                slot->valid = true;
                ++slot->successful_reads;
            } else {
                slot->valid = false;
                slot->last_status = DS18B20_STATUS_OUT_OF_RANGE;
                ++slot->failed_reads;
            }
        } else {
            slot->valid = false;
            ++slot->failed_reads;
        }
        return;
    }

    manager->phase = DS18B20_MANAGER_IDLE;
    manager->next_sample_ms = now + manager->config.sample_interval_ms;
}

void DS18B20_ManagerProcess(DS18B20_Manager *manager)
{
    uint32_t now;

    if ((manager == NULL) || (manager->config.get_time_ms == NULL)) {
        return;
    }
    now = manager->config.get_time_ms(manager->config.time_context);
    if ((manager->phase == DS18B20_MANAGER_IDLE) &&
        deadline_reached(now, manager->next_discovery_ms)) {
        (void)DS18B20_ManagerDiscover(manager);
        manager->next_discovery_ms =
            now + manager->config.discovery_interval_ms;
        return;
    }
    if ((manager->phase == DS18B20_MANAGER_IDLE) &&
        deadline_reached(now, manager->next_sample_ms)) {
        start_conversions(manager, now);
        return;
    }
    if ((manager->phase == DS18B20_MANAGER_CONVERTING) &&
        deadline_reached(now, manager->conversion_deadline_ms)) {
        manager->phase = DS18B20_MANAGER_READING;
        manager->read_index = 0U;
    }
    if (manager->phase == DS18B20_MANAGER_READING) {
        read_one_slot(manager, now);
    }
}
