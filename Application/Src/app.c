/**
 * @file app.c
 * @brief Minimal application used as the clean starting point for new work.
 */
#include "app.h"

#include "diagnostic_led.h"

void App_Init(void)
{
    DiagnosticLed_Init();
    DiagnosticLed_SetMode(DIAGNOSTIC_LED_MODE_DANCE);
}

void App_Process(void)
{
    /* The pattern advances only while the cooperative main loop is alive. */
    DiagnosticLed_Process();
}
