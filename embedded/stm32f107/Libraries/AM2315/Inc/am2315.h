/** @file am2315.h @brief Reusable STM32 HAL I2C driver for AM2315. */
#ifndef AM2315_H
#define AM2315_H

#include "stm32f1xx_hal.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AM2315_DEFAULT_HAL_ADDRESS 0xB8U

typedef enum {
    AM2315_STATUS_OK = 0,
    AM2315_STATUS_INVALID_ARGUMENT,
    AM2315_STATUS_BUS_ERROR,
    AM2315_STATUS_INVALID_RESPONSE,
    AM2315_STATUS_CRC_ERROR
} AM2315_Status;

typedef struct {
    I2C_HandleTypeDef *i2c;
    uint16_t hal_address;
    uint32_t timeout_ms;
} AM2315_Device;

typedef struct {
    float humidity_percent;
    float temperature_c;
} AM2315_Measurement;

/** Bind an initialized I2C handle; no transaction occurs here. */
AM2315_Status AM2315_Init(AM2315_Device *device,
                          I2C_HandleTypeDef *i2c,
                          uint16_t hal_address,
                          uint32_t timeout_ms);

/** Wake the sensor, read humidity and temperature, and verify Modbus CRC16. */
AM2315_Status AM2315_Read(AM2315_Device *device,
                          AM2315_Measurement *measurement);

/** Exposed for protocol tests and validation of captured sensor frames. */
uint16_t AM2315_Crc16(const uint8_t *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* AM2315_H */
