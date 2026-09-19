/**
 * @file one_wire.h
 * @brief Hardware-independent standard-speed 1-Wire master and ROM search.
 *
 * The library never includes an MCU header. A project supplies GPIO, delay,
 * and optional critical-section callbacks in OneWire_Bus. This keeps the
 * protocol reusable on STM32, another microcontroller, or a host-side test.
 */
#ifndef ONE_WIRE_H
#define ONE_WIRE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ONE_WIRE_ROM_SIZE 8U

typedef void (*OneWire_LineAction)(void *context);
typedef bool (*OneWire_ReadLine)(void *context);
typedef void (*OneWire_DelayUs)(void *context, uint32_t microseconds);
typedef uint32_t (*OneWire_EnterCritical)(void *context);
typedef void (*OneWire_ExitCritical)(void *context, uint32_t state);

/** One independent bus binding. A released line requires an external pull-up. */
typedef struct {
    void *context;
    OneWire_LineAction drive_low;
    OneWire_LineAction release_line;
    OneWire_ReadLine read_line;
    OneWire_DelayUs delay_us;
    OneWire_EnterCritical enter_critical;
    OneWire_ExitCritical exit_critical;
} OneWire_Bus;

/** Persistent state used while enumerating every ROM on one bus. */
typedef struct {
    uint8_t rom[ONE_WIRE_ROM_SIZE];
    uint8_t last_discrepancy;
    uint8_t last_family_discrepancy;
    bool last_device;
} OneWire_SearchState;

/** @brief Bind hardware callbacks. @param bus Destination. @param configuration Callbacks. @return true when valid. */
bool OneWire_Init(OneWire_Bus *bus, const OneWire_Bus *configuration);

/** @brief Check mandatory callbacks. @param bus Instance. @return true when usable. */
bool OneWire_IsValid(const OneWire_Bus *bus);

/** @brief Generate reset/presence. @param bus Target bus. @return true when a device responds. */
bool OneWire_Reset(const OneWire_Bus *bus);

/** @brief Write one bit. @param bus Target bus. @param value Bit value. */
void OneWire_WriteBit(const OneWire_Bus *bus, bool value);
/** @brief Sample one bit. @param bus Target bus. @return Sampled value. */
bool OneWire_ReadBit(const OneWire_Bus *bus);

/** @brief Write one LSB-first byte. @param bus Target bus. @param value Byte. */
void OneWire_WriteByte(const OneWire_Bus *bus, uint8_t value);
/** @brief Read one LSB-first byte. @param bus Target bus. @return Byte read. */
uint8_t OneWire_ReadByte(const OneWire_Bus *bus);

/** @brief Calculate Dallas/Maxim CRC8. @param data Input. @param length Byte count. @return CRC8. */
uint8_t OneWire_Crc8(const uint8_t *data, size_t length);

/** @brief Clear search state. @param state State to reset. */
void OneWire_SearchReset(OneWire_SearchState *state);

/** @brief Start Search ROM. @param bus Bus. @param state Caller-owned state. @param rom First valid ROM output. @return true when found. */
bool OneWire_SearchFirst(const OneWire_Bus *bus,
                         OneWire_SearchState *state,
                         uint8_t rom[ONE_WIRE_ROM_SIZE]);

/** @brief Continue Search ROM. @param bus Bus. @param state Existing state. @param rom Next valid ROM output. @return true when found. */
bool OneWire_SearchNext(const OneWire_Bus *bus,
                        OneWire_SearchState *state,
                        uint8_t rom[ONE_WIRE_ROM_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* ONE_WIRE_H */
