/**
 * @file app.h
 * @brief Stable application entry points called by CubeMX-generated main.c.
 *
 * Product logic stays in Application/Src. CubeMX-generated code only passes
 * the initialized SPI handle once and repeatedly calls App_Process().
 */
#ifndef APP_H
#define APP_H

#include "ade7880.h"
#include "stm32f1xx_hal.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Identifies where each voltage conversion coefficient came from. */
typedef enum {
    APP_VOLTAGE_CALIBRATION_NONE = 0, /**< No conversion; inspect raw counts. */
    APP_VOLTAGE_CALIBRATION_LEGACY,   /**< Recovered from the previous firmware. */
    APP_VOLTAGE_CALIBRATION_USER      /**< Replaced with measured board values. */
} App_VoltageCalibrationSource;

/**
 * @brief ADE7880 state intended for the debugger Watch window.
 *
 * A successful read updates raw, voltage_v, and updated_at_ms together. The
 * legacy A/B voltage values are immediately useful for bring-up, but must be
 * checked against a calibrated meter before they are used as final results.
 */
typedef struct {
    bool online; /**< True after startup and the most recent sample succeed. */
    uint8_t die_version; /**< Silicon VERSION register read during startup. */
    ADE7880_Status last_status; /**< Result of the latest init/read operation. */
    uint32_t updated_at_ms; /**< HAL tick of the last complete sample. */
    uint32_t successful_samples; /**< Number of complete snapshots received. */
    uint32_t communication_errors; /**< Failed reads after a valid connection. */
    ADE7880_MeasurementsRaw raw; /**< Uncalibrated values direct from the IC. */
    float voltage_v[ADE7880_PHASE_COUNT]; /**< RMS voltage in volts when valid. */
    bool voltage_valid[ADE7880_PHASE_COUNT]; /**< True when voltage_v is usable. */
    App_VoltageCalibrationSource voltage_source[ADE7880_PHASE_COUNT];
} App_ElectricalState;

/**
 * Live electrical state. Add `g_app_electrical` to the Keil/VS Code debugger
 * Watch window; a complete sample is published only after every SPI read works.
 */
extern volatile App_ElectricalState g_app_electrical;

/**
 * @brief Initialize LEDs and start ADE7880 using an initialized SPI handle.
 * @param ade_spi Address of CubeMX's initialized SPI2 handle.
 *
 * Call once after MX_GPIO_Init() and MX_SPI2_Init(). The function returns even
 * if ADE7880 is absent; App_Process() retries the connection every two seconds.
 */
void App_Init(SPI_HandleTypeDef *ade_spi);

/**
 * @brief Service LEDs and periodic ADE7880 acquisition without a busy wait.
 *
 * Call continuously from while(1). A complete electrical sample is attempted
 * once per second while online.
 */
void App_Process(void);

/**
 * @brief Replace one phase's voltage scale with a measured calibration.
 * @param phase Phase whose AVRMS/BVRMS/CVRMS value is being calibrated.
 * @param calibration Scale and offset, normally calculated from two points.
 * @return ADE7880_STATUS_OK or ADE7880_STATUS_INVALID_ARGUMENT.
 *
 * Example:
 * @code
 * ADE7880_LinearCalibration phase_a;
 * if (ADE7880_CalculateLinearCalibration(raw_low, volts_low,
 *                                        raw_high, volts_high,
 *                                        &phase_a) == ADE7880_STATUS_OK) {
 *     (void)App_SetVoltageCalibration(ADE7880_PHASE_A, &phase_a);
 * }
 * @endcode
 */
ADE7880_Status App_SetVoltageCalibration(
    ADE7880_Phase phase, const ADE7880_LinearCalibration *calibration);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
