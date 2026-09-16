/**
 * @file ade7880.h
 * @brief Reusable STM32 HAL SPI driver for the ADE7880 energy-metering IC.
 *
 * The driver returns explicit status values, owns no global state, performs no
 * calibration, and exposes raw register values. Board-specific CT/PT ratios
 * and engineering-unit conversion belong in the application layer.
 */
#ifndef ADE7880_H
#define ADE7880_H

#include "stm32f1xx_hal.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Driver result independent of the numeric HAL_StatusTypeDef representation. */
typedef enum {
    ADE7880_STATUS_OK = 0,
    ADE7880_STATUS_INVALID_ARGUMENT,
    ADE7880_STATUS_BUS_ERROR,
    ADE7880_STATUS_TIMEOUT
} ADE7880_Status;

/** Three phase selectors used by the convenience measurement API. */
typedef enum {
    ADE7880_PHASE_A = 0,
    ADE7880_PHASE_B,
    ADE7880_PHASE_C
} ADE7880_Phase;

/** Hardware binding and default timeout for one independent ADE7880 device. */
typedef struct {
    SPI_HandleTypeDef *spi;
    GPIO_TypeDef *chip_select_port;
    uint16_t chip_select_pin;
    uint32_t timeout_ms;
} ADE7880_Device;

/** Raw phase quantities; consult the ADE7880 data sheet for scaling. */
typedef struct {
    uint32_t voltage_rms;
    uint32_t current_rms;
    int32_t active_power;
    uint32_t apparent_power;
    int32_t fundamental_power_factor;
} ADE7880_PhaseRaw;

/** Raw three-phase snapshot plus neutral RMS current. */
typedef struct {
    ADE7880_PhaseRaw phase[3];
    uint32_t neutral_current_rms;
} ADE7880_MeasurementsRaw;

/**
 * @brief Bind an already initialized HAL SPI peripheral and chip-select pin.
 * @return OK when every pointer, pin, and timeout is valid.
 * @note This function does not communicate with or reset the IC.
 */
ADE7880_Status ADE7880_Init(ADE7880_Device *device,
                            SPI_HandleTypeDef *spi,
                            GPIO_TypeDef *chip_select_port,
                            uint16_t chip_select_pin,
                            uint32_t timeout_ms);

/** Toggle chip select as required after reset and lock the serial port to SPI. */
ADE7880_Status ADE7880_EnableSpiInterface(ADE7880_Device *device);

/**
 * @brief Read a 1-to-4-byte ADE7880 register in big-endian wire order.
 * @param value Receives a right-aligned unsigned value.
 */
ADE7880_Status ADE7880_ReadRegister(ADE7880_Device *device,
                                    uint16_t address,
                                    uint8_t width,
                                    uint32_t *value);

/** Write the least-significant 1-to-4 bytes of value in big-endian order. */
ADE7880_Status ADE7880_WriteRegister(ADE7880_Device *device,
                                     uint16_t address,
                                     uint8_t width,
                                     uint32_t value);

/** Issue a software reset and stop with TIMEOUT if the reset bit does not clear. */
ADE7880_Status ADE7880_SoftwareReset(ADE7880_Device *device,
                                     uint32_t reset_timeout_ms);

/** Set RUN to one so the digital signal processor starts measurements. */
ADE7880_Status ADE7880_StartMeasurements(ADE7880_Device *device);

/** Read RMS voltage/current, active/apparent power, and fundamental PF. */
ADE7880_Status ADE7880_ReadPhase(ADE7880_Device *device,
                                 ADE7880_Phase phase,
                                 ADE7880_PhaseRaw *measurement);

/** Read all three phase structures and neutral RMS current. */
ADE7880_Status ADE7880_ReadAll(ADE7880_Device *device,
                               ADE7880_MeasurementsRaw *measurements);

#ifdef __cplusplus
}
#endif

#endif /* ADE7880_H */
