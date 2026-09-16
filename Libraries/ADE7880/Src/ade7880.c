/** @file ade7880.c */
#include "ade7880.h"

#include <stdbool.h>
#include <stddef.h>

#define ADE7880_SPI_READ  0x01U
#define ADE7880_SPI_WRITE 0x00U

#define ADE7880_REG_CONFIG2 0xEC01U
#define ADE7880_REG_RUN     0xE228U
#define ADE7880_REG_CONFIG  0xE618U
#define ADE7880_REG_HCONFIG 0xE900U
#define ADE7880_REG_FPF     0xE885U
#define ADE7880_REG_NIRMS   0x43C6U

#define ADE7880_CONFIG_SOFTWARE_RESET (1UL << 7U)
#define ADE7880_HCONFIG_PHASE_MASK     (3UL << 8U)

static const uint16_t voltage_rms_register[3] = {0x43C1U, 0x43C3U, 0x43C5U};
static const uint16_t current_rms_register[3] = {0x43C0U, 0x43C2U, 0x43C4U};
static const uint16_t active_power_register[3] = {0xE513U, 0xE514U, 0xE515U};
static const uint16_t apparent_power_register[3] = {0xE519U, 0xE51AU, 0xE51BU};

/** Confirm that the instance is ready before touching the HAL bus. */
static bool is_valid_device(const ADE7880_Device *device)
{
    return (device != NULL) && (device->spi != NULL) &&
           (device->chip_select_port != NULL) &&
           (device->chip_select_pin != 0U) && (device->timeout_ms != 0U);
}

/** Convert any non-OK HAL transfer result to the public bus-error status. */
static ADE7880_Status from_hal(HAL_StatusTypeDef status)
{
    return (status == HAL_OK) ? ADE7880_STATUS_OK : ADE7880_STATUS_BUS_ERROR;
}

ADE7880_Status ADE7880_Init(ADE7880_Device *device,
                            SPI_HandleTypeDef *spi,
                            GPIO_TypeDef *chip_select_port,
                            uint16_t chip_select_pin,
                            uint32_t timeout_ms)
{
    if ((device == NULL) || (spi == NULL) || (chip_select_port == NULL) ||
        (chip_select_pin == 0U) || (timeout_ms == 0U)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    device->spi = spi;
    device->chip_select_port = chip_select_port;
    device->chip_select_pin = chip_select_pin;
    device->timeout_ms = timeout_ms;
    HAL_GPIO_WritePin(chip_select_port, chip_select_pin, GPIO_PIN_SET);
    return ADE7880_STATUS_OK;
}

ADE7880_Status ADE7880_ReadRegister(ADE7880_Device *device,
                                    uint16_t address,
                                    uint8_t width,
                                    uint32_t *value)
{
    uint8_t command[3] = {
        ADE7880_SPI_READ, (uint8_t)(address >> 8U), (uint8_t)address
    };
    uint8_t bytes[4] = {0U};
    HAL_StatusTypeDef hal_status;
    uint32_t result = 0U;

    if (!is_valid_device(device) || (value == NULL) ||
        (width == 0U) || (width > sizeof(bytes))) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    HAL_GPIO_WritePin(device->chip_select_port, device->chip_select_pin,
                      GPIO_PIN_RESET);
    hal_status = HAL_SPI_Transmit(device->spi, command, sizeof(command),
                                 device->timeout_ms);
    if (hal_status == HAL_OK) {
        hal_status = HAL_SPI_Receive(device->spi, bytes, width,
                                    device->timeout_ms);
    }
    HAL_GPIO_WritePin(device->chip_select_port, device->chip_select_pin,
                      GPIO_PIN_SET);

    if (hal_status != HAL_OK) {
        return from_hal(hal_status);
    }
    for (uint8_t index = 0U; index < width; ++index) {
        result = (result << 8U) | bytes[index];
    }
    *value = result;
    return ADE7880_STATUS_OK;
}

ADE7880_Status ADE7880_WriteRegister(ADE7880_Device *device,
                                     uint16_t address,
                                     uint8_t width,
                                     uint32_t value)
{
    uint8_t frame[7] = {
        ADE7880_SPI_WRITE, (uint8_t)(address >> 8U), (uint8_t)address
    };
    HAL_StatusTypeDef hal_status;

    if (!is_valid_device(device) || (width == 0U) || (width > 4U)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    for (uint8_t index = 0U; index < width; ++index) {
        const uint8_t shift = (uint8_t)((width - 1U - index) * 8U);
        frame[3U + index] = (uint8_t)(value >> shift);
    }

    HAL_GPIO_WritePin(device->chip_select_port, device->chip_select_pin,
                      GPIO_PIN_RESET);
    hal_status = HAL_SPI_Transmit(device->spi, frame, (uint16_t)(width + 3U),
                                 device->timeout_ms);
    HAL_GPIO_WritePin(device->chip_select_port, device->chip_select_pin,
                      GPIO_PIN_SET);
    return from_hal(hal_status);
}

ADE7880_Status ADE7880_EnableSpiInterface(ADE7880_Device *device)
{
    if (!is_valid_device(device)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    /* Three complete high-low-high cycles select SPI after an ADE7880 reset. */
    for (uint8_t pulse = 0U; pulse < 3U; ++pulse) {
        HAL_GPIO_WritePin(device->chip_select_port, device->chip_select_pin,
                          GPIO_PIN_SET);
        HAL_Delay(1U);
        HAL_GPIO_WritePin(device->chip_select_port, device->chip_select_pin,
                          GPIO_PIN_RESET);
        HAL_Delay(1U);
    }
    HAL_GPIO_WritePin(device->chip_select_port, device->chip_select_pin,
                      GPIO_PIN_SET);
    HAL_Delay(1U);
    return ADE7880_WriteRegister(device, ADE7880_REG_CONFIG2, 1U, 0U);
}

ADE7880_Status ADE7880_SoftwareReset(ADE7880_Device *device,
                                     uint32_t reset_timeout_ms)
{
    ADE7880_Status status;
    uint32_t config;
    uint32_t started_ms;

    if (!is_valid_device(device) || (reset_timeout_ms == 0U)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    status = ADE7880_WriteRegister(device, ADE7880_REG_CONFIG, 2U,
                                   ADE7880_CONFIG_SOFTWARE_RESET);
    if (status != ADE7880_STATUS_OK) {
        return status;
    }

    started_ms = HAL_GetTick();
    do {
        status = ADE7880_ReadRegister(device, ADE7880_REG_CONFIG, 2U, &config);
        if (status != ADE7880_STATUS_OK) {
            return status;
        }
        if ((config & ADE7880_CONFIG_SOFTWARE_RESET) == 0U) {
            return ADE7880_STATUS_OK;
        }
    } while ((HAL_GetTick() - started_ms) < reset_timeout_ms);

    return ADE7880_STATUS_TIMEOUT;
}

ADE7880_Status ADE7880_StartMeasurements(ADE7880_Device *device)
{
    return ADE7880_WriteRegister(device, ADE7880_REG_RUN, 2U, 1U);
}

/** Read one raw register and stop the convenience sequence on first error. */
static ADE7880_Status read_value(ADE7880_Device *device,
                                 uint16_t address,
                                 uint32_t *value)
{
    return ADE7880_ReadRegister(device, address, 4U, value);
}

ADE7880_Status ADE7880_ReadPhase(ADE7880_Device *device,
                                 ADE7880_Phase phase,
                                 ADE7880_PhaseRaw *measurement)
{
    ADE7880_Status status;
    uint32_t value;
    uint32_t hconfig;

    if (!is_valid_device(device) || (measurement == NULL) ||
        ((uint32_t)phase > (uint32_t)ADE7880_PHASE_C)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    status = read_value(device, voltage_rms_register[phase], &value);
    if (status != ADE7880_STATUS_OK) return status;
    measurement->voltage_rms = value & 0x00FFFFFFUL;

    status = read_value(device, current_rms_register[phase], &value);
    if (status != ADE7880_STATUS_OK) return status;
    measurement->current_rms = value & 0x00FFFFFFUL;

    status = read_value(device, active_power_register[phase], &value);
    if (status != ADE7880_STATUS_OK) return status;
    measurement->active_power = (int32_t)value;

    status = read_value(device, apparent_power_register[phase], &value);
    if (status != ADE7880_STATUS_OK) return status;
    measurement->apparent_power = value;

    status = ADE7880_ReadRegister(device, ADE7880_REG_HCONFIG, 2U, &hconfig);
    if (status != ADE7880_STATUS_OK) return status;
    hconfig = (hconfig & ~ADE7880_HCONFIG_PHASE_MASK) |
              ((uint32_t)phase << 8U);
    status = ADE7880_WriteRegister(device, ADE7880_REG_HCONFIG, 2U, hconfig);
    if (status != ADE7880_STATUS_OK) return status;
    status = read_value(device, ADE7880_REG_FPF, &value);
    if (status == ADE7880_STATUS_OK) {
        measurement->fundamental_power_factor = (int32_t)value;
    }
    return status;
}

ADE7880_Status ADE7880_ReadAll(ADE7880_Device *device,
                               ADE7880_MeasurementsRaw *measurements)
{
    ADE7880_Status status;

    if (!is_valid_device(device) || (measurements == NULL)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    for (uint32_t phase = 0U; phase < 3U; ++phase) {
        status = ADE7880_ReadPhase(device, (ADE7880_Phase)phase,
                                   &measurements->phase[phase]);
        if (status != ADE7880_STATUS_OK) {
            return status;
        }
    }
    status = read_value(device, ADE7880_REG_NIRMS,
                        &measurements->neutral_current_rms);
    measurements->neutral_current_rms &= 0x00FFFFFFUL;
    return status;
}
