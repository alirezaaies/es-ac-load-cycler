/** @file sensor_address_store.c */
#include "sensor_address_store.h"

#include "one_wire.h"
#include "stm32f1xx_hal.h"

#include <stddef.h>
#include <string.h>

#define STORE_PAGE_ADDRESS   0x0803F800UL
#define STORE_LEGACY_ADDRESS 0x08009000UL
#define STORE_MAGIC          0x524F4D53UL
#define STORE_VERSION        1U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint8_t addresses[SENSOR_ADDRESS_STORE_COUNT][SENSOR_ADDRESS_STORE_ROM_SIZE];
    uint32_t crc32;
} SensorAddressRecord;

/** Reflected IEEE CRC32 protects metadata and all 32 addresses as one unit. */
static uint32_t crc32(const uint8_t *data, uint32_t length)
{
    uint32_t crc = 0xFFFFFFFFUL;

    while (length-- > 0U) {
        crc ^= *data++;
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = 0UL - (crc & 1UL);
            crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
        }
    }
    return ~crc;
}

bool SensorAddressStore_IsValidRom(
    const uint8_t address[SENSOR_ADDRESS_STORE_ROM_SIZE])
{
    return (address != NULL) && (address[0] == 0x28U) &&
           (OneWire_Crc8(address, SENSOR_ADDRESS_STORE_ROM_SIZE - 1U) ==
            address[SENSOR_ADDRESS_STORE_ROM_SIZE - 1U]);
}

static bool is_valid_table(
    const uint8_t addresses[SENSOR_ADDRESS_STORE_COUNT][SENSOR_ADDRESS_STORE_ROM_SIZE])
{
    if (addresses == NULL) {
        return false;
    }
    for (uint32_t index = 0U; index < SENSOR_ADDRESS_STORE_COUNT; ++index) {
        if (!SensorAddressStore_IsValidRom(addresses[index])) {
            return false;
        }
    }
    return true;
}

bool SensorAddressStore_Load(
    uint8_t addresses[SENSOR_ADDRESS_STORE_COUNT][SENSOR_ADDRESS_STORE_ROM_SIZE])
{
    const SensorAddressRecord *record =
        (const SensorAddressRecord *)STORE_PAGE_ADDRESS;
    const uint32_t expected_crc =
        crc32((const uint8_t *)record,
              sizeof(*record) - sizeof(record->crc32));

    if (addresses == NULL) {
        return false;
    }
    if ((record->magic == STORE_MAGIC) &&
        (record->version == STORE_VERSION) &&
        (record->count == SENSOR_ADDRESS_STORE_COUNT) &&
        (record->crc32 == expected_crc) &&
        is_valid_table(record->addresses)) {
        memcpy(addresses, record->addresses, sizeof(record->addresses));
        return true;
    }

    /* Compatibility with the original raw table stored at 0x08009000. */
    const uint8_t (*legacy)[SENSOR_ADDRESS_STORE_ROM_SIZE] =
        (const uint8_t (*)[SENSOR_ADDRESS_STORE_ROM_SIZE])STORE_LEGACY_ADDRESS;
    if (is_valid_table(legacy)) {
        memcpy(addresses, legacy,
               SENSOR_ADDRESS_STORE_COUNT * SENSOR_ADDRESS_STORE_ROM_SIZE);
        (void)SensorAddressStore_Save(addresses);
        return true;
    }
    return false;
}

bool SensorAddressStore_Save(
    const uint8_t addresses[SENSOR_ADDRESS_STORE_COUNT][SENSOR_ADDRESS_STORE_ROM_SIZE])
{
    SensorAddressRecord record;
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;

    if (!is_valid_table(addresses)) {
        return false;
    }
    record.magic = STORE_MAGIC;
    record.version = STORE_VERSION;
    record.count = SENSOR_ADDRESS_STORE_COUNT;
    memcpy(record.addresses, addresses, sizeof(record.addresses));
    record.crc32 = crc32((const uint8_t *)&record,
                         sizeof(record) - sizeof(record.crc32));

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return false;
    }
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = STORE_PAGE_ADDRESS;
    erase.NbPages = 1U;
    status = HAL_FLASHEx_Erase(&erase, &page_error);

    for (uint32_t offset = 0U;
         (status == HAL_OK) && (offset < sizeof(record));
         offset += sizeof(uint32_t)) {
        uint32_t word = 0xFFFFFFFFUL;
        const uint32_t remaining = (uint32_t)sizeof(record) - offset;
        memcpy(&word, ((const uint8_t *)&record) + offset,
               (remaining < sizeof(word)) ? remaining : sizeof(word));
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   STORE_PAGE_ADDRESS + offset, word);
    }
    (void)HAL_FLASH_Lock();
    return status == HAL_OK;
}
