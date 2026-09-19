/** @file sensor_address_store.c */
#include "sensor_address_store.h"

#include "one_wire.h"
#include "stm32f1xx_hal.h"

#include <stddef.h>
#include <string.h>

#define STORE_PAGE_ADDRESS   0x0803F800UL
#define STORE_LEGACY_ADDRESS 0x08009000UL
#define STORE_MAGIC          0x524F4D53UL
#define STORE_VERSION        2U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t payload_size;
    uint8_t payload[SENSOR_ADDRESS_STORE_MAX_PAYLOAD];
    uint32_t crc32;
} SensorAddressRecord;

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t count;
    uint8_t addresses[SENSOR_ADDRESS_STORE_LEGACY_COUNT]
                     [SENSOR_ADDRESS_STORE_ROM_SIZE];
    uint32_t crc32;
} LegacyAddressRecord;

/** Static to keep the 524-byte Flash image off the small embedded stack. */
static SensorAddressRecord write_record;

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

static bool valid_rom(const uint8_t *rom)
{
    return (rom[0] == 0x28U) &&
           (OneWire_Crc8(rom, SENSOR_ADDRESS_STORE_ROM_SIZE - 1U) ==
            rom[SENSOR_ADDRESS_STORE_ROM_SIZE - 1U]);
}

static bool valid_legacy_table(
    const uint8_t addresses[SENSOR_ADDRESS_STORE_LEGACY_COUNT]
                           [SENSOR_ADDRESS_STORE_ROM_SIZE])
{
    for (uint8_t index = 0U; index < SENSOR_ADDRESS_STORE_LEGACY_COUNT;
         ++index) {
        if (!valid_rom(addresses[index])) {
            return false;
        }
    }
    return true;
}

bool SensorAddressStore_Load(void *payload, uint16_t payload_size)
{
    const SensorAddressRecord *record =
        (const SensorAddressRecord *)STORE_PAGE_ADDRESS;

    if ((payload == NULL) || (payload_size == 0U) ||
        (payload_size > SENSOR_ADDRESS_STORE_MAX_PAYLOAD) ||
        (record->magic != STORE_MAGIC) ||
        (record->version != STORE_VERSION) ||
        (record->payload_size != payload_size) ||
        (record->crc32 != crc32((const uint8_t *)record,
                                offsetof(SensorAddressRecord, crc32)))) {
        return false;
    }
    memcpy(payload, record->payload, payload_size);
    return true;
}

bool SensorAddressStore_Save(const void *payload, uint16_t payload_size)
{
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0U;
    HAL_StatusTypeDef status;

    if ((payload == NULL) || (payload_size == 0U) ||
        (payload_size > SENSOR_ADDRESS_STORE_MAX_PAYLOAD)) {
        return false;
    }
    memset(&write_record, 0xFF, sizeof(write_record));
    write_record.magic = STORE_MAGIC;
    write_record.version = STORE_VERSION;
    write_record.payload_size = payload_size;
    memcpy(write_record.payload, payload, payload_size);
    write_record.crc32 = crc32((const uint8_t *)&write_record,
                         offsetof(SensorAddressRecord, crc32));

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) {
        return false;
    }
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = STORE_PAGE_ADDRESS;
    erase.NbPages = 1U;
    status = HAL_FLASHEx_Erase(&erase, &page_error);

    for (uint32_t offset = 0U;
         (status == HAL_OK) && (offset < sizeof(write_record));
         offset += sizeof(uint32_t)) {
        uint32_t word;
        memcpy(&word, ((const uint8_t *)&write_record) + offset,
               sizeof(word));
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   STORE_PAGE_ADDRESS + offset, word);
    }
    (void)HAL_FLASH_Lock();
    if (status != HAL_OK) {
        return false;
    }
    return (memcmp((const void *)STORE_PAGE_ADDRESS, &write_record,
                   sizeof(write_record)) == 0);
}

bool SensorAddressStore_LoadLegacy(
    uint8_t addresses[SENSOR_ADDRESS_STORE_LEGACY_COUNT]
                     [SENSOR_ADDRESS_STORE_ROM_SIZE])
{
    const LegacyAddressRecord *record =
        (const LegacyAddressRecord *)STORE_PAGE_ADDRESS;
    const uint8_t (*raw)[SENSOR_ADDRESS_STORE_ROM_SIZE] =
        (const uint8_t (*)[SENSOR_ADDRESS_STORE_ROM_SIZE])
            STORE_LEGACY_ADDRESS;

    if (addresses == NULL) {
        return false;
    }
    if ((record->magic == STORE_MAGIC) && (record->version == 1U) &&
        (record->count == SENSOR_ADDRESS_STORE_LEGACY_COUNT) &&
        (record->crc32 == crc32((const uint8_t *)record,
                                offsetof(LegacyAddressRecord, crc32))) &&
        valid_legacy_table(record->addresses)) {
        memcpy(addresses, record->addresses, sizeof(record->addresses));
        return true;
    }
    if (valid_legacy_table(raw)) {
        memcpy(addresses, raw,
               SENSOR_ADDRESS_STORE_LEGACY_COUNT *
               SENSOR_ADDRESS_STORE_ROM_SIZE);
        return true;
    }
    return false;
}
