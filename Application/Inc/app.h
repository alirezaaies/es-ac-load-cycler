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

/** Identifies where an engineering-unit conversion coefficient came from. */
typedef enum {
    APP_CALIBRATION_NONE = 0, /**< No conversion; inspect raw counts. */
    APP_CALIBRATION_LEGACY,   /**< Directly recovered from previous firmware. */
    APP_CALIBRATION_DERIVED,  /**< Derived from a documented matched quantity. */
    APP_CALIBRATION_USER      /**< Replaced with measured board values. */
} App_CalibrationSource;

/** Selects one phase quantity for the generic calibration function. */
typedef enum {
    APP_PHASE_QUANTITY_VOLTAGE_RMS = 0, /**< AVRMS/BVRMS/CVRMS to volts. */
    APP_PHASE_QUANTITY_CURRENT_RMS,     /**< AIRMS/BIRMS/CIRMS to amperes. */
    APP_PHASE_QUANTITY_ACTIVE_POWER,    /**< AWATT/BWATT/CWATT to watts. */
    APP_PHASE_QUANTITY_APPARENT_POWER,  /**< AVA/BVA/CVA to volt-amperes. */
    APP_PHASE_QUANTITY_COUNT            /**< Number of phase quantities. */
} App_PhaseQuantity;

/** Selects whether firmware must compensate for a reversed phase CT. */
typedef enum {
    APP_CURRENT_POLARITY_NORMAL = 1, /**< Installed polarity is used unchanged. */
    APP_CURRENT_POLARITY_REVERSED = -1 /**< Correct a CT connected 180 degrees backward. */
} App_CurrentPolarity;

/**
 * @brief ADE7880 state intended for the debugger Watch window.
 *
 * Array index 0/1/2 always means phase A/B/C. A successful read publishes raw
 * registers, calibrated values, PF, validity flags, and timestamp together.
 * Never use an engineering value unless its matching valid flag is true.
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
    App_CalibrationSource voltage_source[ADE7880_PHASE_COUNT];
    float current_a[ADE7880_PHASE_COUNT]; /**< RMS current in amperes when valid. */
    bool current_valid[ADE7880_PHASE_COUNT]; /**< True after current calibration. */
    App_CalibrationSource current_source[ADE7880_PHASE_COUNT];
    float active_power_w[ADE7880_PHASE_COUNT]; /**< Active power in watts. */
    bool active_power_valid[ADE7880_PHASE_COUNT]; /**< True after watt calibration. */
    App_CalibrationSource active_power_source[ADE7880_PHASE_COUNT];
    float apparent_power_va[ADE7880_PHASE_COUNT]; /**< Apparent power in VA. */
    bool apparent_power_valid[ADE7880_PHASE_COUNT]; /**< True after VA calibration. */
    App_CalibrationSource apparent_power_source[ADE7880_PHASE_COUNT];
    float power_factor[ADE7880_PHASE_COUNT]; /**< Signed PF from -1 to +1. */
    float power_factor_abs[ADE7880_PHASE_COUNT]; /**< PF magnitude from 0 to 1. */
    bool power_factor_valid[ADE7880_PHASE_COUNT]; /**< True after a complete read. */
    App_CurrentPolarity current_polarity[ADE7880_PHASE_COUNT];
        /**< Configured CT polarity correction for each phase. */
    float neutral_current_a; /**< Neutral RMS current in amperes when valid. */
    bool neutral_current_valid; /**< True after neutral-current calibration. */
    App_CalibrationSource neutral_current_source; /**< Origin of neutral scale. */
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

/**
 * @brief Install a two-point calibration for one phase measurement.
 * @param quantity Voltage, current, active power, or apparent power.
 * @param phase Phase A, B, or C associated with the raw register.
 * @param calibration Scale and offset calculated from reference measurements.
 * @return ADE7880_STATUS_OK or ADE7880_STATUS_INVALID_ARGUMENT.
 *
 * The selected value becomes valid after the next complete one-second sample.
 * Power factor is not accepted here because its Q1.15 register has a fixed
 * conversion and is published automatically.
 *
 * Example for phase-A current:
 * @code
 * ADE7880_LinearCalibration current_a;
 * if (ADE7880_CalculateLinearCalibration(raw_at_1_a, 1.0F,
 *                                        raw_at_5_a, 5.0F,
 *                                        &current_a) == ADE7880_STATUS_OK) {
 *     (void)App_SetPhaseCalibration(APP_PHASE_QUANTITY_CURRENT_RMS,
 *                                   ADE7880_PHASE_A, &current_a);
 * }
 * @endcode
 */
ADE7880_Status App_SetPhaseCalibration(
    App_PhaseQuantity quantity, ADE7880_Phase phase,
    const ADE7880_LinearCalibration *calibration);

/**
 * @brief Install the neutral-current conversion from NIRMS counts to amperes.
 * @param calibration Scale and offset calculated from two reference points.
 * @return ADE7880_STATUS_OK or ADE7880_STATUS_INVALID_ARGUMENT.
 */
ADE7880_Status App_SetNeutralCurrentCalibration(
    const ADE7880_LinearCalibration *calibration);

/**
 * @brief Configure software compensation for one phase's CT direction.
 * @param phase Phase A, B, or C whose current path is being corrected.
 * @param polarity NORMAL for correct wiring or REVERSED for a backward CT.
 * @return ADE7880_STATUS_OK or ADE7880_STATUS_INVALID_ARGUMENT.
 *
 * This setting changes the sign of active power and signed power factor. RMS
 * current, apparent power, and power-factor magnitude remain positive. Select
 * the setting once during commissioning; do not switch it from live readings.
 *
 * Example for a phase-A CT that cannot be physically reversed:
 * @code
 * (void)App_SetCurrentPolarity(ADE7880_PHASE_A,
 *                              APP_CURRENT_POLARITY_REVERSED);
 * @endcode
 */
ADE7880_Status App_SetCurrentPolarity(ADE7880_Phase phase,
                                      App_CurrentPolarity polarity);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
