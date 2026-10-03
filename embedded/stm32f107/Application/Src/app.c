/**
 * @file app.c
 * @brief ADE7880 integration and cooperative application scheduling.
 */
#include "app.h"
#include "cycler_config.h"

#include "diagnostic_led.h"
#include "ds18b20_manager.h"
#include "main.h"
#include "sensor_address_store.h"
#include "ui_counter.h"

#include <string.h>

#define ADE_SPI_TIMEOUT_MS 20U /**< Maximum time for one HAL SPI operation. */
#define ADE_RESET_TIMEOUT_MS 100U /**< Bounded wait for STATUS1.RSTDONE. */
#define ADE_SAMPLE_INTERVAL_MS 1000U /**< Normal electrical sample period. */
#define ADE_RETRY_INTERVAL_MS 2000U /**< Delay between offline retries. */
#define TEMPERATURE_BUS_COUNT 2U
#define TEMPERATURE_SAMPLE_INTERVAL_MS 250U
#define TEMPERATURE_DISCOVERY_INTERVAL_MS 5000U

/** Public live state; inspect this symbol in the debugger Watch window. */
volatile App_ElectricalState g_app_electrical;

/** Public numbered temperature state; inspect this symbol in debugger Watch. */
static DS18B20_Manager temperature_manager;
volatile float g_temperature_c[APP_TEMPERATURE_SENSOR_COUNT];
volatile bool g_temperature_valid[APP_TEMPERATURE_SENSOR_COUNT];
volatile uint8_t g_temperature_sensor_count;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} App_OneWireGpio;

static App_OneWireGpio temperature_gpio[TEMPERATURE_BUS_COUNT] = {
    {ONE_WIRE_1_GPIO_Port, ONE_WIRE_1_Pin},
    {ONE_WIRE_2_GPIO_Port, ONE_WIRE_2_Pin}
};
static OneWire_Bus temperature_buses[TEMPERATURE_BUS_COUNT];
static bool temperature_manager_ready;

/** Publish the manager's detailed slots through a minimal application view. */
static void publish_temperature_values(void)
{
    uint8_t assigned_count = 0U;

    for (uint8_t index = 0U; index < APP_TEMPERATURE_SENSOR_COUNT; ++index) {
        const DS18B20_SensorSlot *slot = &temperature_manager.slots[index];
        const bool assigned = slot->mapping.assigned == 1U;
        const bool usable = assigned && slot->present && slot->valid;

        if (assigned) {
            ++assigned_count;
        }
        g_temperature_valid[index] = usable;
        g_temperature_c[index] = usable ? slot->temperature_c : 0.0F;
    }
    g_temperature_sensor_count = assigned_count;
}

/*
 * Board startup calibration, written explicitly for easy review and editing.
 * Each entry is {scale, offset} for engineering = raw * scale + offset.
 * Phase-C values currently copy phase B, and neutral current copies the phase
 * current scale, because measured coefficients were not available. These
 * provisional defaults make every channel usable for commissioning, but every
 * populated channel must still be verified and calibrated on its actual board.
 */
static const ADE7880_Calibration default_calibration = {
    .voltage = {
        [ADE7880_PHASE_A] = {0.00055963F * 1.017F, 0.0F},
        [ADE7880_PHASE_B] = {0.00055963F,          0.0F},
        [ADE7880_PHASE_C] = {0.00055963F,          0.0F}
    },
    .current = {
        [ADE7880_PHASE_A] = {0.00036565F / 100.0F, 0.0F},
        [ADE7880_PHASE_B] = {0.00036565F / 100.0F, 0.0F},
        [ADE7880_PHASE_C] = {0.00036565F / 100.0F, 0.0F}
    },
    .active_power = {
        [ADE7880_PHASE_A] = {(0.0170276F / 10.0F) * 1.02F, 0.0F},
        [ADE7880_PHASE_B] = { 0.0170276F / 10.0F,          0.0F},
        [ADE7880_PHASE_C] = { 0.0170276F / 10.0F,          0.0F}
    },
    .apparent_power = {
        [ADE7880_PHASE_A] = {(0.0170276F / 10.0F) * 1.02F, 0.0F},
        [ADE7880_PHASE_B] = { 0.0170276F / 10.0F,          0.0F},
        [ADE7880_PHASE_C] = { 0.0170276F / 10.0F,          0.0F}
    },
    .neutral_current = {0.00036565F / 100.0F, 0.0F}
};

static ADE7880_Device ade_device; /**< Portable driver instance for this IC. */
static SPI_HandleTypeDef *ade_spi_handle; /**< SPI2 handle supplied by main.c. */
static uint32_t next_ade_action_ms; /**< Next sample/retry deadline. */
static ADE7880_Calibration measurement_calibration; /**< Board-specific scales. */
static bool phase_conversion_enabled[APP_PHASE_QUANTITY_COUNT]
                                    [ADE7880_PHASE_COUNT];
static bool neutral_current_conversion_enabled;

/** Refresh optional legacy-style display values without altering SI fields. */
static void update_display_values(void)
{
    for (uint32_t phase = 0U; phase < ADE7880_PHASE_COUNT; ++phase) {
        g_app_electrical.current_display[phase] =
            g_app_electrical.current_valid[phase]
                ? g_app_electrical.current_a[phase] *
                      g_app_electrical.current_display_units_per_a
                : 0.0F;
        g_app_electrical.active_power_display[phase] =
            g_app_electrical.active_power_valid[phase]
                ? g_app_electrical.active_power_w[phase] *
                      g_app_electrical.power_display_units_per_w
                : 0.0F;
        g_app_electrical.apparent_power_display[phase] =
            g_app_electrical.apparent_power_valid[phase]
                ? g_app_electrical.apparent_power_va[phase] *
                      g_app_electrical.power_display_units_per_w
                : 0.0F;
    }
}

/** Restore one complete, symmetric A/B/C calibration set. */
void App_ResetCalibrationToDefaults(void)
{
    measurement_calibration = default_calibration;
    neutral_current_conversion_enabled = true;
    g_app_electrical.neutral_current_a = 0.0F;
    g_app_electrical.neutral_current_valid = false;
    g_app_electrical.neutral_current_source = APP_CALIBRATION_DEFAULT;

    for (uint32_t phase = 0U; phase < ADE7880_PHASE_COUNT; ++phase) {
        for (uint32_t quantity = 0U;
             quantity < (uint32_t)APP_PHASE_QUANTITY_COUNT; ++quantity) {
            phase_conversion_enabled[quantity][phase] = true;
        }
        g_app_electrical.voltage_v[phase] = 0.0F;
        g_app_electrical.current_a[phase] = 0.0F;
        g_app_electrical.active_power_w[phase] = 0.0F;
        g_app_electrical.apparent_power_va[phase] = 0.0F;
        g_app_electrical.voltage_valid[phase] = false;
        g_app_electrical.current_valid[phase] = false;
        g_app_electrical.active_power_valid[phase] = false;
        g_app_electrical.apparent_power_valid[phase] = false;
        g_app_electrical.voltage_source[phase] = APP_CALIBRATION_DEFAULT;
        g_app_electrical.current_source[phase] = APP_CALIBRATION_DEFAULT;
        g_app_electrical.active_power_source[phase] = APP_CALIBRATION_DEFAULT;
        g_app_electrical.apparent_power_source[phase] =
            APP_CALIBRATION_DEFAULT;
    }
    update_display_values();
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

        /*
         * Reversing a CT changes the signs of both active power and signed PF,
         * but it does not change RMS current, apparent power, or PF magnitude.
         */
        if ((g_app_electrical.current_polarity[phase] ==
             APP_CURRENT_POLARITY_REVERSED) &&
            g_app_electrical.active_power_valid[phase]) {
            g_app_electrical.active_power_w[phase] =
                -g_app_electrical.active_power_w[phase];
        }

        /* APF/BPF/CPF use signed Q1.15, so no gain calibration is needed. */
        float signed_pf =
            (float)sample->phase[phase].power_factor_q15 / 32768.0F;
        if (g_app_electrical.current_polarity[phase] ==
            APP_CURRENT_POLARITY_REVERSED) {
            signed_pf = -signed_pf;
        }
        g_app_electrical.power_factor[phase] = signed_pf;
        g_app_electrical.power_factor_abs[phase] =
            (signed_pf < 0.0F) ? -signed_pf : signed_pf;
        g_app_electrical.power_factor_valid[phase] = true;
    }

    publish_linear_value(
        (float)sample->neutral_current_rms,
        &measurement_calibration.neutral_current,
        neutral_current_conversion_enabled,
        &g_app_electrical.neutral_current_a,
        &g_app_electrical.neutral_current_valid);
    update_display_values();
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
    const uint32_t started = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - started) < delay_ms) {
        DiagnosticLed_Process();
        UiCounter_Process();
    }
}

/** Drive one open-drain 1-Wire GPIO low. */
static void one_wire_drive_low(void *context)
{
    const App_OneWireGpio *gpio = (const App_OneWireGpio *)context;
    HAL_GPIO_WritePin(gpio->port, gpio->pin, GPIO_PIN_RESET);
}

/** Release one open-drain GPIO so the external resistor pulls it high. */
static void one_wire_release(void *context)
{
    const App_OneWireGpio *gpio = (const App_OneWireGpio *)context;
    HAL_GPIO_WritePin(gpio->port, gpio->pin, GPIO_PIN_SET);
}

/** Sample the physical 1-Wire level. */
static bool one_wire_read(void *context)
{
    const App_OneWireGpio *gpio = (const App_OneWireGpio *)context;
    return HAL_GPIO_ReadPin(gpio->port, gpio->pin) == GPIO_PIN_SET;
}

/** Accurate short delay backed by the Cortex-M3 cycle counter. */
static void one_wire_delay_us(void *context, uint32_t microseconds)
{
    const uint32_t cycles_per_us = SystemCoreClock / 1000000U;
    const uint32_t started = DWT->CYCCNT;
    const uint32_t target = cycles_per_us * microseconds;

    (void)context;
    while ((DWT->CYCCNT - started) < target) {
    }
}

/** Preserve interrupt state while one timing-sensitive slot is generated. */
static uint32_t one_wire_enter_critical(void *context)
{
    const uint32_t state = __get_PRIMASK();
    (void)context;
    __disable_irq();
    return state;
}

/** Restore the interrupt state captured before the 1-Wire slot. */
static void one_wire_exit_critical(void *context, uint32_t state)
{
    (void)context;
    if (state == 0U) {
        __enable_irq();
    }
}

static uint32_t temperature_time_ms(void *context)
{
    (void)context;
    return HAL_GetTick();
}

static bool temperature_load_mappings(void *context,
                                      DS18B20_MappingEntry *entries,
                                      uint8_t entry_count)
{
    (void)context;
    return SensorAddressStore_Load(entries,
        (uint16_t)((uint16_t)entry_count * sizeof(*entries)));
}

static bool temperature_save_mappings(
    void *context, const DS18B20_MappingEntry *entries, uint8_t entry_count)
{
    (void)context;
    return SensorAddressStore_Save(entries,
        (uint16_t)((uint16_t)entry_count * sizeof(*entries)));
}

/** Bind the portable manager to PB10, PC7, HAL time, and reserved Flash. */
static void initialize_temperature_manager(void)
{
    DS18B20_ManagerConfig manager_config;

    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    for (uint8_t index = 0U; index < TEMPERATURE_BUS_COUNT; ++index) {
        const OneWire_Bus bus_config = {
            .context = &temperature_gpio[index],
            .drive_low = one_wire_drive_low,
            .release_line = one_wire_release,
            .read_line = one_wire_read,
            .delay_us = one_wire_delay_us,
            .enter_critical = one_wire_enter_critical,
            .exit_critical = one_wire_exit_critical
        };
        if (!OneWire_Init(&temperature_buses[index], &bus_config)) {
            temperature_manager_ready = false;
            return;
        }
    }

    memset(&manager_config, 0, sizeof(manager_config));
    manager_config.buses = temperature_buses;
    manager_config.bus_count = TEMPERATURE_BUS_COUNT;
    manager_config.sensor_count = APP_TEMPERATURE_SENSOR_COUNT;
    manager_config.sample_interval_ms = TEMPERATURE_SAMPLE_INTERVAL_MS;
    manager_config.discovery_interval_ms =
        TEMPERATURE_DISCOVERY_INTERVAL_MS;
    manager_config.get_time_ms = temperature_time_ms;
    manager_config.load_mappings = temperature_load_mappings;
    manager_config.save_mappings = temperature_save_mappings;
    temperature_manager_ready =
        DS18B20_ManagerInit(&temperature_manager, &manager_config) ==
        DS18B20_MANAGER_STATUS_OK;
    publish_temperature_values();
}

/** Cooperative ADE startup: wait deadlines instead of delaying the main loop. */
typedef enum { ADE_IDLE, ADE_POWER_WAIT, ADE_SS_PULSE, ADE_LOCK,
               ADE_RESET_REQUEST, ADE_RESET_WAIT, ADE_START, ADE_VERSION } AdeStartup;
static AdeStartup ade_startup;
static uint32_t ade_reset_at;
static uint8_t ade_pulse;
#define APP_ADE_RESET_BIT (1UL << 7U) /**< CONFIG software reset request. */
#define APP_ADE_RESET_DONE_BIT (1UL << 15U) /**< STATUS1 reset completion. */

static void ade_failed(ADE7880_Status status)
{
    g_app_electrical.last_status=status;
    g_app_electrical.online=false;
    g_app_electrical.last_error_status=status;
    g_app_electrical.failed_startup_stage=(uint8_t)ade_startup;
    ++g_app_electrical.startup_errors;
    ade_startup=ADE_IDLE;
    g_app_electrical.startup_stage=(uint8_t)ade_startup;
    ade_select(NULL,false);
    next_ade_action_ms=HAL_GetTick()+ADE_RETRY_INTERVAL_MS;
}

static void initialize_ade(void)
{
    ADE7880_Transport transport={.context=ade_spi_handle,.write=ade_spi_write,
        .read=ade_spi_read,.select=ade_select,.delay_ms=ade_delay};
    ++g_app_electrical.startup_attempts;
    ADE7880_Status status=ADE7880_Init(&ade_device,&transport,ADE_SPI_TIMEOUT_MS);
    if (status!=ADE7880_STATUS_OK) { ade_failed(status); return; }
    ade_startup=ADE_POWER_WAIT;
    g_app_electrical.startup_stage=(uint8_t)ade_startup;
    next_ade_action_ms=HAL_GetTick()+50U;
}

static void service_ade_startup(uint32_t now)
{
    uint32_t value=0U, flags=0U;
    ADE7880_Status status=ADE7880_STATUS_OK;
    next_ade_action_ms=now+1U;
    switch (ade_startup) {
    case ADE_POWER_WAIT:
        ade_pulse=0U; ade_startup=ADE_SS_PULSE; ade_select(NULL,false); break;
    case ADE_SS_PULSE:
        ++ade_pulse;
        ade_select(NULL,(ade_pulse & 1U)!=0U);
        if (ade_pulse==6U) ade_startup=ADE_LOCK;
        break;
    case ADE_LOCK:
        status=ADE7880_WriteRegisterVerified(&ade_device,ADE7880_REG_CONFIG2,1U,0U);
        if (status==ADE7880_STATUS_OK) ade_startup=ADE_RESET_REQUEST;
        break;
    case ADE_RESET_REQUEST:
        status=ADE7880_ReadRegister(&ade_device,ADE7880_REG_CONFIG,2U,&value);
        if (status==ADE7880_STATUS_OK)
            status=ADE7880_WriteRegister(&ade_device,ADE7880_REG_CONFIG,2U,value|APP_ADE_RESET_BIT);
        ade_reset_at=now;
        if (status==ADE7880_STATUS_OK) ade_startup=ADE_RESET_WAIT;
        break;
    case ADE_RESET_WAIT:
        status=ADE7880_ReadRegister(&ade_device,ADE7880_REG_CONFIG,2U,&value);
        if (status==ADE7880_STATUS_OK)
            status=ADE7880_ReadRegister(&ade_device,ADE7880_REG_STATUS1,4U,&flags);
        if (status==ADE7880_STATUS_OK && (value&APP_ADE_RESET_BIT)==0U && (flags&APP_ADE_RESET_DONE_BIT)!=0U) {
            status=ADE7880_WriteRegister(&ade_device,ADE7880_REG_STATUS1,4U,APP_ADE_RESET_DONE_BIT);
            if (status==ADE7880_STATUS_OK) ade_startup=ADE_START;
        } else if ((uint32_t)(now-ade_reset_at)>=ADE_RESET_TIMEOUT_MS)
            status=ADE7880_STATUS_TIMEOUT;
        break;
    case ADE_START:
        status=ADE7880_StartMeasurements(&ade_device);
        if (status==ADE7880_STATUS_OK) ade_startup=ADE_VERSION;
        break;
    case ADE_VERSION:
        status=ADE7880_ReadRegister(&ade_device,ADE7880_REG_VERSION,1U,&value);
        if (status==ADE7880_STATUS_OK) {
            g_app_electrical.die_version=(uint8_t)value;
            g_app_electrical.online=true; ade_startup=ADE_IDLE;
            next_ade_action_ms=now;
        }
        break;
    default: break;
    }
    g_app_electrical.startup_stage=(uint8_t)ade_startup;
    g_app_electrical.last_status=status;
    if (status!=ADE7880_STATUS_OK) ade_failed(status);
}

/** Initialize application state, default three-phase scaling, LEDs, and ADE7880. */
void App_Init(SPI_HandleTypeDef *ade_spi)
{
    DiagnosticLed_Init();
    DiagnosticLed_SetMode(DIAGNOSTIC_LED_MODE_DANCE);
    UiCounter_Init();
    /* Temperature libraries retained, but discovery/Flash writes are disabled
       in the dedicated electrical load-cycler application. */
    if (CYCLER_ENABLE_TEMPERATURE) initialize_temperature_manager();
    memset((void *)&g_app_electrical, 0, sizeof(g_app_electrical));
    memset(&measurement_calibration, 0, sizeof(measurement_calibration));
    memset(phase_conversion_enabled, 0, sizeof(phase_conversion_enabled));
    neutral_current_conversion_enabled = false;
    g_app_electrical.current_display_units_per_a =
        APP_DEFAULT_CURRENT_DISPLAY_UNITS_PER_AMP;
    g_app_electrical.power_display_units_per_w =
        APP_DEFAULT_POWER_DISPLAY_UNITS_PER_WATT;
    for (uint32_t phase = 0U; phase < ADE7880_PHASE_COUNT; ++phase) {
        g_app_electrical.current_polarity[phase] =
            APP_CURRENT_POLARITY_NORMAL;
    }
    App_ResetCalibrationToDefaults();
    ade_spi_handle = ade_spi;

    if (ade_spi_handle == NULL) {
        g_app_electrical.last_status = ADE7880_STATUS_INVALID_ARGUMENT;
        /* LED heartbeat continues even when electrical data is unavailable. */
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
    UiCounter_Process();
    if (temperature_manager_ready) {
        if (DS18B20_ManagerProcess(&temperature_manager)) {
            publish_temperature_values();
        }
    }
    if (ade_spi_handle == NULL) {
        return;
    }
    /* Signed comparison remains safe when the millisecond counter wraps. */
    if ((int32_t)(now - next_ade_action_ms) < 0) {
        return;
    }

    if (ade_startup != ADE_IDLE) {
        service_ade_startup(now);
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
        g_app_electrical.updated_at_ms = HAL_GetTick();
        ++g_app_electrical.successful_samples;
        next_ade_action_ms = now + ADE_SAMPLE_INTERVAL_MS;
    } else {
        g_app_electrical.online = false;
        ++g_app_electrical.communication_errors;
        g_app_electrical.last_error_status=status;
        /* LED heartbeat continues even when electrical data is unavailable. */
        next_ade_action_ms = now + ADE_RETRY_INTERVAL_MS;
    }
}

bool App_TemperatureGetCelsius(uint8_t sensor_number, float *temperature_c)
{
    uint8_t index;

    if ((sensor_number == 0U) ||
        (sensor_number > APP_TEMPERATURE_SENSOR_COUNT) ||
        (temperature_c == NULL)) {
        return false;
    }
    index = (uint8_t)(sensor_number - 1U);
    if (!g_temperature_valid[index]) {
        return false;
    }
    *temperature_c = g_temperature_c[index];
    return true;
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
        g_app_electrical.voltage_v[phase] = 0.0F;
        g_app_electrical.voltage_valid[phase] = false;
        g_app_electrical.voltage_source[phase] =
            APP_CALIBRATION_USER;
        break;
    case APP_PHASE_QUANTITY_CURRENT_RMS:
        g_app_electrical.current_a[phase] = 0.0F;
        g_app_electrical.current_valid[phase] = false;
        g_app_electrical.current_source[phase] = APP_CALIBRATION_USER;
        break;
    case APP_PHASE_QUANTITY_ACTIVE_POWER:
        g_app_electrical.active_power_w[phase] = 0.0F;
        g_app_electrical.active_power_valid[phase] = false;
        g_app_electrical.active_power_source[phase] = APP_CALIBRATION_USER;
        break;
    case APP_PHASE_QUANTITY_APPARENT_POWER:
        g_app_electrical.apparent_power_va[phase] = 0.0F;
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
    update_display_values();
    return ADE7880_STATUS_OK;
}

/** Fit and install one SI conversion from multiple operating points. */
ADE7880_Status App_CalibratePhaseMultiPoint(
    App_PhaseQuantity quantity, ADE7880_Phase phase,
    const ADE7880_CalibrationPoint *points, size_t point_count,
    ADE7880_LinearCalibration *calibration_result,
    float *max_abs_error)
{
    ADE7880_LinearCalibration calibration;
    ADE7880_Status status;

    if (((uint32_t)quantity >= (uint32_t)APP_PHASE_QUANTITY_COUNT) ||
        ((uint32_t)phase >= (uint32_t)ADE7880_PHASE_COUNT)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }
    status = ADE7880_CalculateLinearCalibrationMultiPoint(
        points, point_count, &calibration, max_abs_error);

    if (status != ADE7880_STATUS_OK) {
        return status;
    }
    /* References describe the final published sign, after CT correction. */
    if ((quantity == APP_PHASE_QUANTITY_ACTIVE_POWER) &&
        (g_app_electrical.current_polarity[phase] ==
         APP_CURRENT_POLARITY_REVERSED)) {
        calibration.scale = -calibration.scale;
        calibration.offset = -calibration.offset;
    }
    status = App_SetPhaseCalibration(quantity, phase, &calibration);
    if ((status == ADE7880_STATUS_OK) && (calibration_result != NULL)) {
        *calibration_result = calibration;
    }
    return status;
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
    g_app_electrical.neutral_current_a = 0.0F;
    g_app_electrical.neutral_current_valid = false;
    g_app_electrical.neutral_current_source = APP_CALIBRATION_USER;
    return ADE7880_STATUS_OK;
}

/** Fit and install one neutral-current conversion from multiple points. */
ADE7880_Status App_CalibrateNeutralCurrentMultiPoint(
    const ADE7880_CalibrationPoint *points, size_t point_count,
    ADE7880_LinearCalibration *calibration_result,
    float *max_abs_error)
{
    ADE7880_LinearCalibration calibration;
    ADE7880_Status status = ADE7880_CalculateLinearCalibrationMultiPoint(
        points, point_count, &calibration, max_abs_error);

    if (status != ADE7880_STATUS_OK) {
        return status;
    }
    status = App_SetNeutralCurrentCalibration(&calibration);
    if ((status == ADE7880_STATUS_OK) && (calibration_result != NULL)) {
        *calibration_result = calibration;
    }
    return status;
}

/** Configure a persistent runtime correction for a reversed phase CT. */
ADE7880_Status App_SetCurrentPolarity(ADE7880_Phase phase,
                                      App_CurrentPolarity polarity)
{
    if (((uint32_t)phase >= (uint32_t)ADE7880_PHASE_COUNT) ||
        ((polarity != APP_CURRENT_POLARITY_NORMAL) &&
         (polarity != APP_CURRENT_POLARITY_REVERSED))) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    g_app_electrical.current_polarity[phase] = polarity;
    /* Signed values are stale until they are rebuilt from the next snapshot. */
    g_app_electrical.active_power_w[phase] = 0.0F;
    g_app_electrical.active_power_valid[phase] = false;
    g_app_electrical.power_factor[phase] = 0.0F;
    g_app_electrical.power_factor_abs[phase] = 0.0F;
    g_app_electrical.power_factor_valid[phase] = false;
    update_display_values();
    return ADE7880_STATUS_OK;
}

/** Change display multipliers while preserving calibrated SI measurements. */
ADE7880_Status App_SetDisplayUnits(float current_units_per_a,
                                   float power_units_per_w)
{
    if ((current_units_per_a <= 0.0F) || (power_units_per_w <= 0.0F)) {
        return ADE7880_STATUS_INVALID_ARGUMENT;
    }

    g_app_electrical.current_display_units_per_a = current_units_per_a;
    g_app_electrical.power_display_units_per_w = power_units_per_w;
    update_display_values();
    return ADE7880_STATUS_OK;
}
