/** @file one_wire.c */
#include "one_wire.h"

#include <stddef.h>

/** A valid bus has a GPIO, one pin mask, and an accurate delay provider. */
static bool is_valid(const OneWire_Bus *bus)
{
    return (bus != NULL) && (bus->port != NULL) &&
           (bus->pin != 0U) && (bus->delay_us != NULL);
}

/** Open-drain high releases the line to its mandatory external pull-up. */
static void release_line(const OneWire_Bus *bus)
{
    HAL_GPIO_WritePin(bus->port, bus->pin, GPIO_PIN_SET);
}

static void pull_low(const OneWire_Bus *bus)
{
    HAL_GPIO_WritePin(bus->port, bus->pin, GPIO_PIN_RESET);
}

bool OneWire_Init(OneWire_Bus *bus,
                  GPIO_TypeDef *port,
                  uint16_t pin,
                  OneWire_DelayUs delay_us)
{
    GPIO_InitTypeDef gpio = {0};

    if ((bus == NULL) || (port == NULL) || (pin == 0U) || (delay_us == NULL)) {
        return false;
    }
    bus->port = port;
    bus->pin = pin;
    bus->delay_us = delay_us;

    release_line(bus);
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(port, &gpio);
    release_line(bus);
    return true;
}

bool OneWire_Reset(const OneWire_Bus *bus)
{
    bool presence;

    if (!is_valid(bus)) {
        return false;
    }
    pull_low(bus);
    bus->delay_us(480U);
    release_line(bus);
    bus->delay_us(70U);
    presence = (HAL_GPIO_ReadPin(bus->port, bus->pin) == GPIO_PIN_RESET);
    bus->delay_us(410U);
    return presence;
}

void OneWire_WriteBit(const OneWire_Bus *bus, bool value)
{
    if (!is_valid(bus)) {
        return;
    }
    pull_low(bus);
    if (value) {
        bus->delay_us(6U);
        release_line(bus);
        bus->delay_us(64U);
    } else {
        bus->delay_us(60U);
        release_line(bus);
        bus->delay_us(10U);
    }
}

bool OneWire_ReadBit(const OneWire_Bus *bus)
{
    bool value;

    if (!is_valid(bus)) {
        return false;
    }
    pull_low(bus);
    bus->delay_us(6U);
    release_line(bus);
    bus->delay_us(9U);
    value = (HAL_GPIO_ReadPin(bus->port, bus->pin) == GPIO_PIN_SET);
    bus->delay_us(55U);
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

uint8_t OneWire_Crc8(const uint8_t *data, uint32_t length)
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
