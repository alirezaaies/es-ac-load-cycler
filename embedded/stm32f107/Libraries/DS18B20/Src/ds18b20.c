/** @file ds18b20.c */
#include "ds18b20.h"

#include <stddef.h>

#define ONE_WIRE_COMMAND_READ_ROM  0x33U
#define ONE_WIRE_COMMAND_MATCH_ROM 0x55U
#define ONE_WIRE_COMMAND_SKIP_ROM  0xCCU
#define DS18B20_COMMAND_CONVERT     0x44U
#define DS18B20_COMMAND_WRITE_PAD   0x4EU
#define DS18B20_COMMAND_READ_PAD    0xBEU

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

static bool is_valid_resolution(DS18B20_Resolution resolution)
{
    return (resolution >= DS18B20_RESOLUTION_9_BIT) &&
           (resolution <= DS18B20_RESOLUTION_12_BIT);
}

static uint8_t resolution_configuration(DS18B20_Resolution resolution)
{
    return (uint8_t)(0x1FU |
                     ((uint8_t)(resolution - DS18B20_RESOLUTION_9_BIT) << 5U));
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
    if (!OneWire_IsValid(bus) || (rom == NULL)) {
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
    if (!OneWire_IsValid(bus) ||
        ((rom != NULL) && !DS18B20_IsValidRom(rom))) {
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
    return OneWire_IsValid(bus) && OneWire_ReadBit(bus);
}

DS18B20_Status DS18B20_ReadScratchpad(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE])
{
    if (!OneWire_IsValid(bus) || !DS18B20_IsValidRom(rom) ||
        (scratchpad == NULL)) {
        return DS18B20_STATUS_INVALID_ARGUMENT;
    }
    if (!OneWire_Reset(bus)) {
        return DS18B20_STATUS_NO_DEVICE;
    }
    select_device(bus, rom);
    OneWire_WriteByte(bus, DS18B20_COMMAND_READ_PAD);
    for (uint8_t index = 0U; index < DS18B20_SCRATCHPAD_SIZE; ++index) {
        scratchpad[index] = OneWire_ReadByte(bus);
    }
    return (OneWire_Crc8(scratchpad, DS18B20_SCRATCHPAD_SIZE - 1U) ==
            scratchpad[DS18B20_SCRATCHPAD_SIZE - 1U])
               ? DS18B20_STATUS_OK
               : DS18B20_STATUS_CRC_ERROR;
}

DS18B20_Status DS18B20_WriteScratchpad(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    int8_t alarm_high_c,
    int8_t alarm_low_c,
    DS18B20_Resolution resolution)
{
    if (!OneWire_IsValid(bus) || !DS18B20_IsValidRom(rom) ||
        !is_valid_resolution(resolution)) {
        return DS18B20_STATUS_INVALID_ARGUMENT;
    }
    if (!OneWire_Reset(bus)) {
        return DS18B20_STATUS_NO_DEVICE;
    }
    select_device(bus, rom);
    OneWire_WriteByte(bus, DS18B20_COMMAND_WRITE_PAD);
    OneWire_WriteByte(bus, (uint8_t)alarm_high_c);
    OneWire_WriteByte(bus, (uint8_t)alarm_low_c);
    OneWire_WriteByte(bus, resolution_configuration(resolution));
    return DS18B20_STATUS_OK;
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
    return (float)raw_temperature / 16.0F;
}

DS18B20_Resolution DS18B20_GetResolution(
    const uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE])
{
    if (scratchpad == NULL) {
        return DS18B20_RESOLUTION_12_BIT;
    }
    return (DS18B20_Resolution)(DS18B20_RESOLUTION_9_BIT +
        ((scratchpad[4] >> 5U) & 0x03U));
}

uint16_t DS18B20_GetConversionTimeMs(DS18B20_Resolution resolution)
{
    switch (resolution) {
    case DS18B20_RESOLUTION_9_BIT:
        return 94U;
    case DS18B20_RESOLUTION_10_BIT:
        return 188U;
    case DS18B20_RESOLUTION_11_BIT:
        return 375U;
    case DS18B20_RESOLUTION_12_BIT:
    default:
        return 750U;
    }
}

DS18B20_Status DS18B20_ReadTemperatureBlocking(
    const OneWire_Bus *bus,
    const uint8_t rom[DS18B20_ROM_SIZE],
    uint32_t timeout_ms,
    float *temperature_c)
{
    uint8_t scratchpad[DS18B20_SCRATCHPAD_SIZE];
    DS18B20_Status status;

    if (!OneWire_IsValid(bus) || !DS18B20_IsValidRom(rom) ||
        (timeout_ms == 0U) || (temperature_c == NULL)) {
        return DS18B20_STATUS_INVALID_ARGUMENT;
    }
    status = DS18B20_StartConversion(bus, rom);
    if (status != DS18B20_STATUS_OK) {
        return status;
    }
    while (!DS18B20_IsConversionReady(bus)) {
        if (timeout_ms-- == 0U) {
            return DS18B20_STATUS_TIMEOUT;
        }
        bus->delay_us(bus->context, 1000U);
    }
    status = DS18B20_ReadScratchpad(bus, rom, scratchpad);
    if (status == DS18B20_STATUS_OK) {
        const float value = DS18B20_RawToCelsius(
            DS18B20_DecodeRaw(scratchpad));
        if ((value < -55.0F) || (value > 125.0F)) {
            return DS18B20_STATUS_OUT_OF_RANGE;
        }
        *temperature_c = value;
    }
    return status;
}
