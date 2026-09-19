/** @file one_wire.c */
#include "one_wire.h"

#include <string.h>

#define ONE_WIRE_SEARCH_ROM 0xF0U

static uint32_t enter_critical(const OneWire_Bus *bus)
{
    return (bus->enter_critical != NULL)
               ? bus->enter_critical(bus->context)
               : 0U;
}

static void exit_critical(const OneWire_Bus *bus, uint32_t state)
{
    if (bus->exit_critical != NULL) {
        bus->exit_critical(bus->context, state);
    }
}

bool OneWire_IsValid(const OneWire_Bus *bus)
{
    return (bus != NULL) && (bus->drive_low != NULL) &&
           (bus->release_line != NULL) && (bus->read_line != NULL) &&
           (bus->delay_us != NULL);
}

bool OneWire_Init(OneWire_Bus *bus, const OneWire_Bus *configuration)
{
    if ((bus == NULL) || !OneWire_IsValid(configuration)) {
        return false;
    }
    *bus = *configuration;
    bus->release_line(bus->context);
    return true;
}

bool OneWire_Reset(const OneWire_Bus *bus)
{
    bool presence;
    uint32_t critical_state;

    if (!OneWire_IsValid(bus)) {
        return false;
    }
    critical_state = enter_critical(bus);
    bus->drive_low(bus->context);
    bus->delay_us(bus->context, 480U);
    bus->release_line(bus->context);
    bus->delay_us(bus->context, 70U);
    presence = !bus->read_line(bus->context);
    bus->delay_us(bus->context, 410U);
    exit_critical(bus, critical_state);
    return presence;
}

void OneWire_WriteBit(const OneWire_Bus *bus, bool value)
{
    uint32_t critical_state;

    if (!OneWire_IsValid(bus)) {
        return;
    }
    critical_state = enter_critical(bus);
    bus->drive_low(bus->context);
    if (value) {
        bus->delay_us(bus->context, 6U);
        bus->release_line(bus->context);
        bus->delay_us(bus->context, 64U);
    } else {
        bus->delay_us(bus->context, 60U);
        bus->release_line(bus->context);
        bus->delay_us(bus->context, 10U);
    }
    exit_critical(bus, critical_state);
}

bool OneWire_ReadBit(const OneWire_Bus *bus)
{
    bool value;
    uint32_t critical_state;

    if (!OneWire_IsValid(bus)) {
        return false;
    }
    critical_state = enter_critical(bus);
    bus->drive_low(bus->context);
    bus->delay_us(bus->context, 3U);
    bus->release_line(bus->context);
    bus->delay_us(bus->context, 10U);
    value = bus->read_line(bus->context);
    bus->delay_us(bus->context, 53U);
    exit_critical(bus, critical_state);
    return value;
}

void OneWire_WriteByte(const OneWire_Bus *bus, uint8_t value)
{
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        OneWire_WriteBit(bus, ((value >> bit) & 1U) != 0U);
    }
}

uint8_t OneWire_ReadByte(const OneWire_Bus *bus)
{
    uint8_t value = 0U;

    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        if (OneWire_ReadBit(bus)) {
            value |= (uint8_t)(1U << bit);
        }
    }
    return value;
}

uint8_t OneWire_Crc8(const uint8_t *data, size_t length)
{
    uint8_t crc = 0U;

    if (data == NULL) {
        return 0U;
    }
    while (length-- > 0U) {
        uint8_t value = *data++;
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            const uint8_t mix = (uint8_t)((crc ^ value) & 1U);
            crc >>= 1U;
            if (mix != 0U) {
                crc ^= 0x8CU;
            }
            value >>= 1U;
        }
    }
    return crc;
}

void OneWire_SearchReset(OneWire_SearchState *state)
{
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
    }
}

static bool search_next(const OneWire_Bus *bus, OneWire_SearchState *state)
{
    uint8_t bit_number = 1U;
    uint8_t last_zero = 0U;
    uint8_t rom_byte_number = 0U;
    uint8_t rom_byte_mask = 1U;

    if (!OneWire_IsValid(bus) || (state == NULL) || state->last_device) {
        return false;
    }
    if (!OneWire_Reset(bus)) {
        OneWire_SearchReset(state);
        return false;
    }
    OneWire_WriteByte(bus, ONE_WIRE_SEARCH_ROM);

    while (rom_byte_number < ONE_WIRE_ROM_SIZE) {
        const bool id_bit = OneWire_ReadBit(bus);
        const bool complement_bit = OneWire_ReadBit(bus);
        bool direction;

        if (id_bit && complement_bit) {
            break;
        }
        if (id_bit != complement_bit) {
            direction = id_bit;
        } else {
            if (bit_number < state->last_discrepancy) {
                direction = (state->rom[rom_byte_number] & rom_byte_mask) != 0U;
            } else {
                direction = bit_number == state->last_discrepancy;
            }
            if (!direction) {
                last_zero = bit_number;
                if (last_zero < 9U) {
                    state->last_family_discrepancy = last_zero;
                }
            }
        }

        if (direction) {
            state->rom[rom_byte_number] |= rom_byte_mask;
        } else {
            state->rom[rom_byte_number] &= (uint8_t)~rom_byte_mask;
        }
        OneWire_WriteBit(bus, direction);

        ++bit_number;
        rom_byte_mask <<= 1U;
        if (rom_byte_mask == 0U) {
            ++rom_byte_number;
            rom_byte_mask = 1U;
        }
    }

    if ((bit_number != 65U) ||
        (OneWire_Crc8(state->rom, ONE_WIRE_ROM_SIZE - 1U) !=
         state->rom[ONE_WIRE_ROM_SIZE - 1U])) {
        OneWire_SearchReset(state);
        return false;
    }
    state->last_discrepancy = last_zero;
    state->last_device = last_zero == 0U;
    return true;
}

bool OneWire_SearchFirst(const OneWire_Bus *bus,
                         OneWire_SearchState *state,
                         uint8_t rom[ONE_WIRE_ROM_SIZE])
{
    if ((state == NULL) || (rom == NULL)) {
        return false;
    }
    OneWire_SearchReset(state);
    return OneWire_SearchNext(bus, state, rom);
}

bool OneWire_SearchNext(const OneWire_Bus *bus,
                        OneWire_SearchState *state,
                        uint8_t rom[ONE_WIRE_ROM_SIZE])
{
    if ((state == NULL) || (rom == NULL) || !search_next(bus, state)) {
        return false;
    }
    memcpy(rom, state->rom, ONE_WIRE_ROM_SIZE);
    return true;
}
