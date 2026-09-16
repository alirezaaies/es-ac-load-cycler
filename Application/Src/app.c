/**
 * @file app.c
 * @brief ADE7880 integration and cooperative application scheduling.
 */
#include "app.h"

#include "diagnostic_led.h"
#include "main.h"

#include <string.h>

#define ADE_SPI_TIMEOUT_MS 20U /**< Maximum time for one HAL SPI operation. */
#define ADE_RESET_TIMEOUT_MS 100U /**< Bounded wait for STATUS1.RSTDONE. */
#define ADE_SAMPLE_INTERVAL_MS 1000U /**< Normal electrical sample period. */
#define ADE_RETRY_INTERVAL_MS 2000U /**< Delay between offline retries. */

/*
 * Voltage conversion recovered from the previous working firmware. It turns
 * AVRMS/BVRMS register counts into approximate volts for immediate bring-up.
 * These are board-specific legacy values, not universal ADE7880 constants.
 */
#define LEGACY_VOLTAGE_SCALE_V_PER_COUNT 0.00055963F
#define LEGACY_PHASE_A_CORRECTION 1.017F
#define LEGACY_CURRENT_SCALE_A_PER_COUNT 0.00036565F
#define LEGACY_POWER_SCALE_W_PER_COUNT 0.0170276F
#define LEGACY_PHASE_A_POWER_CORRECTION 1.02F

/** Public live state; inspect this symbol in the debugger Watch window. */
volatile App_ElectricalState g_app_electrical;

static ADE7880_Device ade_device; /**< Portable driver instance for this IC. */
static SPI_HandleTypeDef *ade_spi_handle; /**< SPI2 handle supplied by main.c. */
static uint32_t next_ade_action_ms; /**< Next sample/retry deadline. */
static ADE7880_Calibration measurement_calibration; /**< Board-specific scales. */
static bool phase_conversion_enabled[APP_PHASE_QUANTITY_COUNT]
                                    [ADE7880_PHASE_COUNT];
static bool neutral_current_conversion_enabled;

/** Load the A/B scales recovered from the previous firmware for this board. */
static void load_legacy_calibration(void)
{
    measurement_calibration.voltage[ADE7880_PHASE_A].scale =
        LEGACY_VOLTAGE_SCALE_V_PER_COUNT * LEGACY_PHASE_A_CORRECTION;
    measurement_calibration.voltage[ADE7880_PHASE_A].offset = 0.0F;
    phase_conversion_enabled[APP_PHASE_QUANTITY_VOLTAGE_RMS]
                            [ADE7880_PHASE_A] = true;
    g_app_electrical.voltage_source[ADE7880_PHASE_A] =
        APP_CALIBRATION_LEGACY;

    measurement_calibration.voltage[ADE7880_PHASE_B].scale =
        LEGACY_VOLTAGE_SCALE_V_PER_COUNT;
    measurement_calibration.voltage[ADE7880_PHASE_B].offset = 0.0F;
    phase_conversion_enabled[APP_PHASE_QUANTITY_VOLTAGE_RMS]
                            [ADE7880_PHASE_B] = true;
    g_app_electrical.voltage_source[ADE7880_PHASE_B] =
        APP_CALIBRATION_LEGACY;

    for (uint32_t phase = ADE7880_PHASE_A; phase <= ADE7880_PHASE_B; ++phase) {
        measurement_calibration.current[phase].scale =
            LEGACY_CURRENT_SCALE_A_PER_COUNT;
        measurement_calibration.current[phase].offset = 0.0F;
        phase_conversion_enabled[APP_PHASE_QUANTITY_CURRENT_RMS][phase] = true;
        g_app_electrical.current_source[phase] = APP_CALIBRATION_LEGACY;

        measurement_calibration.active_power[phase].scale =
            LEGACY_POWER_SCALE_W_PER_COUNT;
        measurement_calibration.active_power[phase].offset = 0.0F;
        phase_conversion_enabled[APP_PHASE_QUANTITY_ACTIVE_POWER][phase] = true;
        g_app_electrical.active_power_source[phase] = APP_CALIBRATION_LEGACY;

        /* ADE7880 active/apparent powers are gain matched on each phase. */
        measurement_calibration.apparent_power[phase] =
            measurement_calibration.active_power[phase];
        phase_conversion_enabled[APP_PHASE_QUANTITY_APPARENT_POWER][phase] =
            true;
        g_app_electrical.apparent_power_source[phase] =
            APP_CALIBRATION_DERIVED;
    }

    measurement_calibration.active_power[ADE7880_PHASE_A].scale *=
        LEGACY_PHASE_A_POWER_CORRECTION;
    measurement_calibration.apparent_power[ADE7880_PHASE_A].scale *=
        LEGACY_PHASE_A_POWER_CORRECTION;

    /* Phase C and neutral had no old conversion, so their sources stay NONE. */
}

/** Publish one calibrated field or explicitly mark it unavailable. */
static void publish_linear_value(float raw,
                                 const ADE7880_LinearCalibration *calibration,
                                 bool enabled, volatile float *value,
                                 volatile bool *valid)
{
    float converted = 0.0F;

    if (enabled &&
        (ADE7880_ApplyLinearCalibration(raw, calibration, &converted) ==
         ADE7880_STATUS_OK)) {
        *value = converted;
        *valid = true;
    } else {
        *value = 0.0F;
        *valid = false;
    }
}

/** Convert calibrated quantities and decode the fixed-format power factor. */
static void update_engineering_values(const ADE7880_MeasurementsRaw *sample)
{
    for (uint32_t phase = 0U; phase < ADE7880_PHASE_COUNT; ++phase) {
        publish_linear_value(
            (float)sample->phase[phase].voltage_rms,
            &measurement_calibration.voltage[phase],
            phase_conversion_enabled[APP_PHASE_QUANTITY_VOLTAGE_RMS][phase],
            &g_app_electrical.voltage_v[phase],
            &g_app_electrical.voltage_valid[phase]);
        publish_linear_value(
            (float)sample->phase[phase].current_rms,
            &measurement_calibration.current[phase],
            phase_conversion_enabled[APP_PHASE_QUANTITY_CURRENT_RMS][phase],
            &g_app_electrical.current_a[phase],
            &g_app_electrical.current_valid[phase]);
        publish_linear_value(
            (float)sample->phase[phase].active_power,
            &measurement_calibration.active_power[phase],
            phase_conversion_enabled[APP_PHASE_QUANTITY_ACTIVE_POWER][phase],
            &g_app_electrical.active_power_w[phase],
            &g_app_electrical.active_power_valid[phase]);
        publish_linear_value(
            (float)sample->phase[phase].apparent_power,
            &measurement_calibration.apparent_power[phase],
            phase_conversion_enabled[APP_PHASE_QUANTITY_APPARENT_POWER][phase],
            &g_app_electrical.apparent_power_va[phase],
            &g_app_electrical.apparent_power_valid[phase]);

        /* APF/BPF/CPF use signed Q1.15, so no board-specific scale is needed. */
        g_app_electrical.power_factor[phase] =
            (float)sample->phase[phase].power_factor_q15 / 32768.0F;
        g_app_electrical.power_factor_valid[phase] = true;
    }

    publish_linear_value(
        (float)sample->neutral_current_rms,
        &measurement_calibration.neutral_current,
        neutral_current_conversion_enabled,
        &g_app_electrical.neutral_current_a,
        &g_app_electrical.neutral_current_valid);
}

/** Map STM32 HAL transfer results to the portable driver status values. */
static ADE7880_Status from_hal(HAL_StatusTypeDef status)
{
    if (status == HAL_OK) return ADE7880_STATUS_OK;
    if (status == HAL_TIMEOUT) return ADE7880_STATUS_TIMEOUT;
    return ADE7880_STATUS_BUS_ERROR;
}

/** STM32 adapter for the portable driver's byte-oriented SPI write callback. */
static ADE7880_Status ade_spi_write(void *context, const uint8_t *data,
                                    size_t length, uint32_t timeout_ms)
{
    SPI_HandleTypeDef *spi = (SPI_HandleTypeDef *)context;

    if ((spi == NULL) || (data == NULL) || (length > UINT16_MAX)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    /* HAL's API is not const-correct; it does not modify the transmit bytes. */
    return from_hal(HAL_SPI_Transmit(spi, (uint8_t *)data, (uint16_t)length,
                                     timeout_ms));
}

/** STM32 adapter for the response bytes clocked after a read command. */
static ADE7880_Status ade_spi_read(void *context, uint8_t *data,
                                   size_t length, uint32_t timeout_ms)
{
    SPI_HandleTypeDef *spi = (SPI_HandleTypeDef *)context;

    if ((spi == NULL) || (data == NULL) || (length > UINT16_MAX)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    return from_hal(HAL_SPI_Receive(spi, data, (uint16_t)length, timeout_ms));
}

/** Drive the active-low ADE7880 SS/HSA line. */
static void ade_select(void *context, bool active)
{
    (void)context;
    HAL_GPIO_WritePin(ADE_CS_GPIO_Port, ADE_CS_Pin,
                      active ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/** Supply the short startup/reset delays required by the ADE7880 data sheet. */
static void ade_delay(void *context, uint32_t delay_ms)
{
    (void)context;
    HAL_Delay(delay_ms);
}

/** Try a complete initialization and publish a clear LED/debugger result. */
static void initialize_ade(void)
{
    /* This is the only STM32-specific binding required by the ADE library. */
    ADE7880_Transport transport = {
        .context = ade_spi_handle,
        .write = ade_spi_write,
        .read = ade_spi_read,
        .select = ade_select,
        .delay_ms = ade_delay
    };
    uint32_t version = 0U;
    ADE7880_Status status;

    /* Bind callbacks, perform the complete IC startup, then prove one read. */
    status = ADE7880_Init(&ade_device, &transport, ADE_SPI_TIMEOUT_MS);
    if (status == ADE7880_STATUS_OK) {
        status = ADE7880_Begin(&ade_device, ADE_RESET_TIMEOUT_MS);
    }
    if (status == ADE7880_STATUS_OK) {
        status = ADE7880_ReadRegister(&ade_device, ADE7880_REG_VERSION, 1U,
                                      &version);
    }

    g_app_electrical.last_status = status;
    g_app_electrical.online = (status == ADE7880_STATUS_OK);
    g_app_electrical.die_version = (uint8_t)version;
    DiagnosticLed_SetMode(g_app_electrical.online
                              ? DIAGNOSTIC_LED_MODE_DANCE
                              : DIAGNOSTIC_LED_MODE_ERROR);
    next_ade_action_ms = HAL_GetTick() + (g_app_electrical.online
                              ? ADE_SAMPLE_INTERVAL_MS
                              : ADE_RETRY_INTERVAL_MS);
}

/** Initialize application state, legacy voltage scaling, LEDs, and ADE7880. */
void App_Init(SPI_HandleTypeDef *ade_spi)
{
    DiagnosticLed_Init();
    memset((void *)&g_app_electrical, 0, sizeof(g_app_electrical));
    memset(&measurement_calibration, 0, sizeof(measurement_calibration));
    memset(phase_conversion_enabled, 0, sizeof(phase_conversion_enabled));
    neutral_current_conversion_enabled = false;
    load_legacy_calibration();
    ade_spi_handle = ade_spi;

    if (ade_spi_handle == NULL) {
        g_app_electrical.last_status = ADE7880_STATUS_INVALID_ARGUMENT;
        DiagnosticLed_SetMode(DIAGNOSTIC_LED_MODE_ERROR);
        return;
    }
    initialize_ade();
}

/** Service the cooperative one-second measurement/retry state machine. */
void App_Process(void)
{
    ADE7880_MeasurementsRaw sample;
    ADE7880_Status status;
    const uint32_t now = HAL_GetTick();

    DiagnosticLed_Process();
    if (ade_spi_handle == NULL) {
        return;
    }
    /* Signed comparison remains safe when the millisecond counter wraps. */
    if ((int32_t)(now - next_ade_action_ms) < 0) {
        return;
    }

    if (!g_app_electrical.online) {
        initialize_ade();
        return;
    }

    /* Read into a local object so a failed transaction cannot corrupt live data. */
    status = ADE7880_ReadAll(&ade_device, &sample);
    g_app_electrical.last_status = status;
    if (status == ADE7880_STATUS_OK) {
        g_app_electrical.raw = sample;
        update_engineering_values(&sample);
        g_app_electrical.updated_at_ms = now;
        ++g_app_electrical.successful_samples;
        next_ade_action_ms = now + ADE_SAMPLE_INTERVAL_MS;
    } else {
        g_app_electrical.online = false;
        ++g_app_electrical.communication_errors;
        DiagnosticLed_SetMode(DIAGNOSTIC_LED_MODE_ERROR);
        next_ade_action_ms = now + ADE_RETRY_INTERVAL_MS;
    }
}

/** Install a measured voltage calibration for one phase. */
ADE7880_Status App_SetVoltageCalibration(
    ADE7880_Phase phase, const ADE7880_LinearCalibration *calibration)
{
    return App_SetPhaseCalibration(APP_PHASE_QUANTITY_VOLTAGE_RMS, phase,
                                   calibration);
}

/** Return the calibration storage associated with one phase quantity. */
static ADE7880_LinearCalibration *phase_calibration_slot(
    App_PhaseQuantity quantity, ADE7880_Phase phase)
{
    switch (quantity) {
    case APP_PHASE_QUANTITY_VOLTAGE_RMS:
        return &measurement_calibration.voltage[phase];
    case APP_PHASE_QUANTITY_CURRENT_RMS:
        return &measurement_calibration.current[phase];
    case APP_PHASE_QUANTITY_ACTIVE_POWER:
        return &measurement_calibration.active_power[phase];
    case APP_PHASE_QUANTITY_APPARENT_POWER:
        return &measurement_calibration.apparent_power[phase];
    default:
        return NULL;
    }
}

/** Mark the selected debugger value stale until the next complete sample. */
static void invalidate_phase_value(App_PhaseQuantity quantity,
                                   ADE7880_Phase phase)
{
    switch (quantity) {
    case APP_PHASE_QUANTITY_VOLTAGE_RMS:
        g_app_electrical.voltage_valid[phase] = false;
        g_app_electrical.voltage_source[phase] =
            APP_CALIBRATION_USER;
        break;
    case APP_PHASE_QUANTITY_CURRENT_RMS:
        g_app_electrical.current_valid[phase] = false;
        g_app_electrical.current_source[phase] = APP_CALIBRATION_USER;
        break;
    case APP_PHASE_QUANTITY_ACTIVE_POWER:
        g_app_electrical.active_power_valid[phase] = false;
        g_app_electrical.active_power_source[phase] = APP_CALIBRATION_USER;
        break;
    case APP_PHASE_QUANTITY_APPARENT_POWER:
        g_app_electrical.apparent_power_valid[phase] = false;
        g_app_electrical.apparent_power_source[phase] = APP_CALIBRATION_USER;
        break;
    default:
        break;
    }
}

/** Install a measured scale for voltage, current, active power, or VA. */
ADE7880_Status App_SetPhaseCalibration(
    App_PhaseQuantity quantity, ADE7880_Phase phase,
    const ADE7880_LinearCalibration *calibration)
{
    ADE7880_LinearCalibration *slot;

    if (((uint32_t)quantity >= (uint32_t)APP_PHASE_QUANTITY_COUNT) ||
        ((uint32_t)phase >= (uint32_t)ADE7880_PHASE_COUNT) ||
        (calibration == NULL)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    slot = phase_calibration_slot(quantity, phase);
    if (slot == NULL) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    /* The new pair becomes active on the next successful one-second sample. */
    *slot = *calibration;
    phase_conversion_enabled[quantity][phase] = true;
    invalidate_phase_value(quantity, phase);
    return ADE7880_STATUS_OK;
}

/** Install a measured scale for the neutral RMS current register. */
ADE7880_Status App_SetNeutralCurrentCalibration(
    const ADE7880_LinearCalibration *calibration)
{
    if (calibration == NULL) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    measurement_calibration.neutral_current = *calibration;
    neutral_current_conversion_enabled = true;
    g_app_electrical.neutral_current_valid = false;
    g_app_electrical.neutral_current_source = APP_CALIBRATION_USER;
    return ADE7880_STATUS_OK;
}
