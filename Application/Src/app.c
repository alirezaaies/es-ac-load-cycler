/**
 * @file app.c
 * @brief ADE7880 integration and cooperative application scheduling.
 */
#include "app.h"

#include "diagnostic_led.h"
#include "main.h"

#include <string.h>

#define ADE_SPI_TIMEOUT_MS 20U
#define ADE_RESET_TIMEOUT_MS 100U
#define ADE_SAMPLE_INTERVAL_MS 1000U
#define ADE_RETRY_INTERVAL_MS 2000U

volatile App_ElectricalState g_app_electrical;

static ADE7880_Device ade_device;
static SPI_HandleTypeDef *ade_spi_handle;
static uint32_t next_ade_action_ms;

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
    ADE7880_Transport transport = {
        .context = ade_spi_handle,
        .write = ade_spi_write,
        .read = ade_spi_read,
        .select = ade_select,
        .delay_ms = ade_delay
    };
    uint32_t version = 0U;
    ADE7880_Status status;

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

void App_Init(SPI_HandleTypeDef *ade_spi)
{
    DiagnosticLed_Init();
    memset((void *)&g_app_electrical, 0, sizeof(g_app_electrical));
    ade_spi_handle = ade_spi;

    if (ade_spi_handle == NULL) {
        g_app_electrical.last_status = ADE7880_STATUS_INVALID_ARGUMENT;
        DiagnosticLed_SetMode(DIAGNOSTIC_LED_MODE_ERROR);
        return;
    }
    initialize_ade();
}

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

    status = ADE7880_ReadAll(&ade_device, &sample);
    g_app_electrical.last_status = status;
    if (status == ADE7880_STATUS_OK) {
        g_app_electrical.raw = sample;
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
