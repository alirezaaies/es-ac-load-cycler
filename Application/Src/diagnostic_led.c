/**
 * @file diagnostic_led.c
 * @brief Small tick-driven LED sequencer with no delays or interrupts.
 */
#include "diagnostic_led.h"

#include <stdbool.h>
#include <stddef.h>

#define DIAGNOSTIC_LED_STEP_MS 125U
#define DIAGNOSTIC_ERROR_STEP_MS 150U
#define LED_1_MASK 0x01U
#define LED_2_MASK 0x02U

static const uint8_t heartbeat_pattern[] = {
    LED_1_MASK | LED_2_MASK, 0U, LED_1_MASK, 0U, 0U, 0U, 0U, 0U
};

static const uint8_t dance_pattern[] = {
    LED_1_MASK, 0U, LED_2_MASK, 0U,
    LED_1_MASK | LED_2_MASK, 0U, 0U, 0U
};

static const uint8_t error_pattern[] = {LED_1_MASK, LED_2_MASK};

static bool initialized;
static DiagnosticLed_Mode mode = DIAGNOSTIC_LED_MODE_OFF;
static uint8_t step_index;
static uint32_t last_step_ms;

/** Translate a logical LED state to the configured electrical polarity. */
static GPIO_PinState gpio_state(bool on)
{
#if DIAGNOSTIC_LED_ACTIVE_LOW
    return on ? GPIO_PIN_RESET : GPIO_PIN_SET;
#else
    return on ? GPIO_PIN_SET : GPIO_PIN_RESET;
#endif
}

/** Write one two-bit pattern value to the two independent GPIO pins. */
static void write_pattern(uint8_t pattern)
{
    HAL_GPIO_WritePin(DIAGNOSTIC_LED_1_PORT, DIAGNOSTIC_LED_1_PIN,
                      gpio_state((pattern & LED_1_MASK) != 0U));
    HAL_GPIO_WritePin(DIAGNOSTIC_LED_2_PORT, DIAGNOSTIC_LED_2_PIN,
                      gpio_state((pattern & LED_2_MASK) != 0U));
}

/** Resolve a public mode to its immutable sequence and timing metadata. */
static const uint8_t *get_pattern(DiagnosticLed_Mode selected_mode,
                                  uint8_t *length,
                                  uint32_t *period_ms)
{
    switch (selected_mode) {
    case DIAGNOSTIC_LED_MODE_HEARTBEAT:
        *length = (uint8_t)(sizeof(heartbeat_pattern) / sizeof(heartbeat_pattern[0]));
        *period_ms = DIAGNOSTIC_LED_STEP_MS;
        return heartbeat_pattern;
    case DIAGNOSTIC_LED_MODE_DANCE:
        *length = (uint8_t)(sizeof(dance_pattern) / sizeof(dance_pattern[0]));
        *period_ms = DIAGNOSTIC_LED_STEP_MS;
        return dance_pattern;
    case DIAGNOSTIC_LED_MODE_ERROR:
        *length = (uint8_t)(sizeof(error_pattern) / sizeof(error_pattern[0]));
        *period_ms = DIAGNOSTIC_ERROR_STEP_MS;
        return error_pattern;
    case DIAGNOSTIC_LED_MODE_OFF:
    default:
        *length = 0U;
        *period_ms = 0U;
        return NULL;
    }
}

void DiagnosticLed_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    gpio.Pin = DIAGNOSTIC_LED_1_PIN | DIAGNOSTIC_LED_2_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &gpio);

    initialized = true;
    mode = DIAGNOSTIC_LED_MODE_OFF;
    step_index = 0U;
    last_step_ms = HAL_GetTick();
    write_pattern(0U);
}

void DiagnosticLed_SetMode(DiagnosticLed_Mode new_mode)
{
    uint8_t length;
    uint32_t period_ms;
    const uint8_t *pattern;

    if (!initialized) {
        DiagnosticLed_Init();
    }
    if (new_mode > DIAGNOSTIC_LED_MODE_ERROR) {
        new_mode = DIAGNOSTIC_LED_MODE_OFF;
    }

    mode = new_mode;
    step_index = 0U;
    last_step_ms = HAL_GetTick();
    pattern = get_pattern(mode, &length, &period_ms);
    write_pattern((pattern != NULL) ? pattern[0] : 0U);
}

void DiagnosticLed_Process(void)
{
    uint8_t length;
    uint32_t period_ms;
    const uint8_t *pattern;
    uint32_t elapsed_ms;
    uint32_t elapsed_steps;

    if (!initialized) {
        DiagnosticLed_Init();
    }

    pattern = get_pattern(mode, &length, &period_ms);
    if ((pattern == NULL) || (length == 0U) || (period_ms == 0U)) {
        write_pattern(0U);
        return;
    }

    /* Unsigned subtraction remains correct when the 32-bit HAL tick wraps. */
    elapsed_ms = HAL_GetTick() - last_step_ms;
    if (elapsed_ms < period_ms) {
        return;
    }

    elapsed_steps = elapsed_ms / period_ms;
    step_index = (uint8_t)((step_index + elapsed_steps) % length);
    last_step_ms += elapsed_steps * period_ms;
    write_pattern(pattern[step_index]);
}

DiagnosticLed_Mode DiagnosticLed_GetMode(void)
{
    return mode;
}
