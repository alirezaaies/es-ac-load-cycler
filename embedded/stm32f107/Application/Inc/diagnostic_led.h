/**
 * @file diagnostic_led.h
 * @brief Non-blocking two-LED run and fault indicator.
 */
#ifndef DIAGNOSTIC_LED_H
#define DIAGNOSTIC_LED_H

#include "stm32f1xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Set to 0 for LEDs wired from the MCU pin through a resistor to ground. */
#ifndef DIAGNOSTIC_LED_ACTIVE_LOW
#define DIAGNOSTIC_LED_ACTIVE_LOW 1U
#endif

#define DIAGNOSTIC_LED_1_PORT GPIOC       /**< GPIO bank for diagnostic LED 1. */
#define DIAGNOSTIC_LED_1_PIN  GPIO_PIN_13 /**< Pin mask for diagnostic LED 1. */
#define DIAGNOSTIC_LED_2_PORT GPIOC       /**< GPIO bank for diagnostic LED 2. */
#define DIAGNOSTIC_LED_2_PIN  GPIO_PIN_14 /**< Pin mask for diagnostic LED 2. */

/** Available visual states for normal operation, testing, and fatal errors. */
typedef enum {
    DIAGNOSTIC_LED_MODE_OFF = 0, /**< Both LEDs remain off. */
    DIAGNOSTIC_LED_MODE_HEARTBEAT, /**< Short periodic alive indication. */
    DIAGNOSTIC_LED_MODE_DANCE, /**< Normal-operation two-LED sequence. */
    DIAGNOSTIC_LED_MODE_ERROR /**< Fast alternating communication fault. */
} DiagnosticLed_Mode;

/** Configure PC13/PC14 as low-speed outputs and turn both LEDs off. */
void DiagnosticLed_Init(void);

/** Advance the selected pattern using HAL_GetTick() without blocking. */
void DiagnosticLed_Process(void);

/**
 * @brief Select a visual pattern and restart it from the first step.
 * @param mode One value from DiagnosticLed_Mode; invalid values select OFF.
 */
void DiagnosticLed_SetMode(DiagnosticLed_Mode mode);

/** @return The mode most recently accepted by DiagnosticLed_SetMode(). */
DiagnosticLed_Mode DiagnosticLed_GetMode(void);

#ifdef __cplusplus
}
#endif

#endif /* DIAGNOSTIC_LED_H */
