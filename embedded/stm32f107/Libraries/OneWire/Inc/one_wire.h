/**
 * @file one_wire.h
 * @brief GPIO-based, hardware-independent 1-Wire master for STM32 HAL.
 */
#ifndef ONE_WIRE_H
#define ONE_WIRE_H

#include "stm32f1xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Application-supplied accurate blocking microsecond delay callback. */
typedef void (*OneWire_DelayUs)(uint32_t microseconds);

/** One independent 1-Wire bus binding; no global GPIO is assumed. */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    OneWire_DelayUs delay_us;
} OneWire_Bus;

/** Configure an externally pulled-up pin as released open-drain output. */
bool OneWire_Init(OneWire_Bus *bus,
                  GPIO_TypeDef *port,
                  uint16_t pin,
                  OneWire_DelayUs delay_us);

/** Generate a reset pulse and return true when at least one device responds. */
bool OneWire_Reset(const OneWire_Bus *bus);

/** Write or sample one standard-speed 1-Wire time slot. */
void OneWire_WriteBit(const OneWire_Bus *bus, bool value);
bool OneWire_ReadBit(const OneWire_Bus *bus);

/** Write or read one byte, least-significant bit first as required by 1-Wire. */
void OneWire_WriteByte(const OneWire_Bus *bus, uint8_t value);
uint8_t OneWire_ReadByte(const OneWire_Bus *bus);

/** Dallas/Maxim reflected CRC8 used by ROM codes and scratchpads. */
uint8_t OneWire_Crc8(const uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* ONE_WIRE_H */
