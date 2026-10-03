/** @file ui_counter.h @brief LCD and three-button load cycler. */
#ifndef UI_COUNTER_H
#define UI_COUNTER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile uint32_t g_ui_counter_value;

/** @brief Initialize relays OFF, LCD and load-cycler state.
 * GPIO and the HAL tick must already be initialized. */
void UiCounter_Init(void);
/** @brief Service debounce and pending UI events without blocking.
 * Call continuously from the cooperative main loop. */
void UiCounter_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_COUNTER_H */
