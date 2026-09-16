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

/** ADE7880 state intended for the debugger Watch window and future protocol. */
typedef struct {
    bool online;
    uint8_t die_version;
    ADE7880_Status last_status;
    uint32_t updated_at_ms;
    uint32_t successful_samples;
    uint32_t communication_errors;
    ADE7880_MeasurementsRaw raw;
} App_ElectricalState;

/**
 * Live electrical state. Add `g_app_electrical` to the Keil/VS Code debugger
 * Watch window; a complete sample is published only after every SPI read works.
 */
extern volatile App_ElectricalState g_app_electrical;

/** Initialize LEDs and bind the already initialized CubeMX SPI2 handle. */
void App_Init(SPI_HandleTypeDef *ade_spi);

/** Run the non-blocking LED service and periodic ADE7880 acquisition. */
void App_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
