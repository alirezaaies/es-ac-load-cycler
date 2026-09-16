/** @file am2315.c */
#include "am2315.h"

#include <stddef.h>

#define AM2315_READ_REGISTERS 0x03U
#define AM2315_RESPONSE_SIZE  8U

uint16_t AM2315_Crc16(const uint8_t *data, uint32_t length)
{
    uint16_t crc = 0xFFFFU;

    if (data == NULL) {
        return 0U;
    }
    while (length-- > 0U) {
        crc ^= *data++;
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            crc = ((crc & 1U) != 0U)
                      ? (uint16_t)((crc >> 1U) ^ 0xA001U)
                      : (uint16_t)(crc >> 1U);
        }
    }
    return crc;
}

AM2315_Status AM2315_Init(AM2315_Device *device,
                          I2C_HandleTypeDef *i2c,
                          uint16_t hal_address,
                          uint32_t timeout_ms)
{
    if ((device == NULL) || (i2c == NULL) ||
        (hal_address == 0U) || (timeout_ms == 0U)) {
        return AM2315_STATUS_INVALID_ARGUMENT;
    }
    device->i2c = i2c;
    device->hal_address = hal_address;
    device->timeout_ms = timeout_ms;
    return AM2315_STATUS_OK;
}

AM2315_Status AM2315_Read(AM2315_Device *device,
                          AM2315_Measurement *measurement)
{
    uint8_t command[3] = {AM2315_READ_REGISTERS, 0U, 4U};
    uint8_t response[AM2315_RESPONSE_SIZE] = {0U};
    HAL_StatusTypeDef hal_status;
    uint16_t received_crc;
    int16_t temperature_raw;

    if ((device == NULL) || (device->i2c == NULL) ||
        (device->hal_address == 0U) || (device->timeout_ms == 0U) ||
        (measurement == NULL)) {
        return AM2315_STATUS_INVALID_ARGUMENT;
    }

    /* The address probe is used only to wake the sleeping sensor. */
    hal_status = HAL_I2C_IsDeviceReady(device->i2c, device->hal_address,
                                      1U, device->timeout_ms);
    if (hal_status != HAL_OK) {
        return AM2315_STATUS_BUS_ERROR;
    }
    HAL_Delay(2U);
    hal_status = HAL_I2C_Master_Transmit(device->i2c, device->hal_address,
                                        command, sizeof(command),
                                        device->timeout_ms);
    if (hal_status != HAL_OK) {
        return AM2315_STATUS_BUS_ERROR;
    }
    HAL_Delay(2U);
    hal_status = HAL_I2C_Master_Receive(device->i2c, device->hal_address,
                                       response, sizeof(response),
                                       device->timeout_ms);
    if (hal_status != HAL_OK) {
        return AM2315_STATUS_BUS_ERROR;
    }
    if ((response[0] != AM2315_READ_REGISTERS) || (response[1] != 4U)) {
        return AM2315_STATUS_INVALID_RESPONSE;
    }

    received_crc = (uint16_t)response[6] | ((uint16_t)response[7] << 8U);
    if (AM2315_Crc16(response, 6U) != received_crc) {
        return AM2315_STATUS_CRC_ERROR;
    }

    measurement->humidity_percent =
        (float)(((uint16_t)response[2] << 8U) | response[3]) / 10.0f;
    temperature_raw =
        (int16_t)(((uint16_t)(response[4] & 0x7FU) << 8U) | response[5]);
    if ((response[4] & 0x80U) != 0U) {
        temperature_raw = (int16_t)-temperature_raw;
    }
    measurement->temperature_c = (float)temperature_raw / 10.0f;
    return AM2315_STATUS_OK;
}
