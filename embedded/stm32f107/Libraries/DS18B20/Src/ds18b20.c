/** @file ds18b20.c */
#include "ds18b20.h"

#include <stddef.h>

#define ONE_WIRE_COMMAND_READ_ROM    0x33U
#define ONE_WIRE_COMMAND_MATCH_ROM   0x55U
#define ONE_WIRE_COMMAND_SKIP_ROM    0xCCU
#define DS18B20_COMMAND_CONVERT      0x44U
#define DS18B20_COMMAND_SCRATCHPAD   0xBEU

/** Select one addressed sensor, or broadcast when no ROM is supplied. */
static void select_device(const OneWire_Bus *bus, const uint8_t *rom)
{
    if (rom == NULL) {
        OneWire_WriteByte(bus, ONE_WIRE_COMMAND_SKIP_ROM);
        return;
    }
    OneWire_WriteByte(bus, ONE_WIRE_COMMAND_MATCH_ROM);
    for (uint8_t index = 0U; index < DS18B20_ROM_SIZE; ++index) {
        OneWire_WriteByte(bus, rom[index]);
    }
}

bool DS18B20_IsValidRom(const uint8_t rom[DS18B20_ROM_SIZE])
{
    return (rom != NULL) && (rom[0] == DS18B20_FAMILY_CODE) &&
           (OneWire_Crc8(rom, DS18B20_ROM_SIZE - 1U) ==
            rom[DS18B20_ROM_SIZE - 1U]);
}

DS18B20_Status DS18B20_ReadRom(const OneWire_Bus *bus,
                               uint8_t rom[DS18B20_ROM_SIZE])
{
    if ((bus == NULL) || (rom == NULL)) {
        return DS18B20_STATUS_INVALID_ARGUMENT;
    }
    if (!OneWire_Reset(bus)) {
        return DS18B20_STATUS_NO_DEVICE;
    }
    OneWire_WriteByte(bus, ONE_WIRE_COMMAND_READ_ROM);
    for (uint8_t index = 0U; index < DS18B20_ROM_SIZE; ++index) {
        rom[index] = OneWire_ReadByte(bus);
    }
    return DS18B20_IsValidRom(rom) ? DS18B20_STATUS_OK
                                   : DS18B20_STATUS_CRC_ERROR;
}

DS18B20_Status DS18B20_StartConversion(const OneWire_Bus *bus,
                                       const uint8_t rom[DS18B20_ROM_SIZE])
{
    if ((bus == NULL) || ((rom != NULL) && !DS18B20_IsValidRom(rom))) {
        return DS18B20_STATUS_INVALID_ARGUMENT;
    }
    if (!OneWire_Reset(bus)) {
        return DS18B20_STATUS_NO_DEVICE;
    }
    select_device(bus, rom);
    OneWire_WriteByte(bus, DS18B20_COMMAND_CONVERT);
    return DS18B20_STATUS_OK;
}

bool DS18B20_IsConversionReady(const OneWire_Bus *bus)
{
    return (bus != NULL) && OneWire_ReadBit(bus);
}

DS18B20_Status DS18B20_ReadScratchpad(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE])
{
    if ((bus == NULL) || !DS18B20_IsValidRom(rom) || (scratchpad == NULL)) {
        return DS18B20_STATUS_INVALID_ARGUMENT;
    }
    if (!OneWire_Reset(bus)) {
        return DS18B20_STATUS_NO_DEVICE;
    }
    select_device(bus, rom);
    OneWire_WriteByte(bus, DS18B20_COMMAND_SCRATCHPAD);
    for (uint8_t index = 0U; index < DS18B20_SCRATCHPAD_SIZE; ++index) {
        scratchpad[index] = OneWire_ReadByte(bus);
    }
    return (OneWire_Crc8(scratchpad, DS18B20_SCRATCHPAD_SIZE - 1U) ==
            scratchpad[DS18B20_SCRATCHPAD_SIZE - 1U])
               ? DS18B20_STATUS_OK
               : DS18B20_STATUS_CRC_ERROR;
}

int16_t DS18B20_DecodeRaw(const uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE])
{
    if (scratchpad == NULL) {
        return 0;
    }
    return (int16_t)(((uint16_t)scratchpad[1] << 8U) | scratchpad[0]);
}

float DS18B20_RawToCelsius(int16_t raw_temperature)
{
    return (float)raw_temperature / 16.0f;
}

DS18B20_Status DS18B20_ReadTemperature(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    uint32_t timeout_ms,
    float *temperature_c)
{
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE];
    uint32_t started_ms;
    DS18B20_Status status;

    if ((bus == NULL) || !DS18B20_IsValidRom(rom) ||
        (timeout_ms == 0U) || (temperature_c == NULL)) {
        return DS18B20_STATUS_INVALID_ARGUMENT;
    }
    status = DS18B20_StartConversion(bus, rom);
    if (status != DS18B20_STATUS_OK) {
        return status;
    }

    started_ms = HAL_GetTick();
    while (!DS18B20_IsConversionReady(bus)) {
        if ((HAL_GetTick() - started_ms) >= timeout_ms) {
            return DS18B20_STATUS_TIMEOUT;
        }
        bus->delay_us(1000U);
    }

    status = DS18B20_ReadScratchpad(bus, rom, scratchpad);
    if (status == DS18B20_STATUS_OK) {
        *temperature_c = DS18B20_RawToCelsius(
            DS18B20_DecodeRaw(scratchpad));
    }
    return status;
}
