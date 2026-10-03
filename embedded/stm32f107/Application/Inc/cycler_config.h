/** @file cycler_config.h @brief Editable load-cycler timing and display policy. */
#ifndef CYCLER_CONFIG_H
#define CYCLER_CONFIG_H
#define CYCLER_MENU_HOLD_MS 3000U /**< Hold K3 to enter/cancel; milliseconds. */
#define CYCLER_DEBOUNCE_MS 30U /**< Stable electrical level; milliseconds. */
#define CYCLER_REPEAT_DELAY_MS 500U /**< Delay before held-key repeat; ms. */
#define CYCLER_REPEAT_MS 120U /**< Held-key repeat interval; milliseconds. */
#define CYCLER_SPLASH_MS 2000U /**< Duration of each startup page; ms. */
#define CYCLER_MAX_COUNT 100000U /**< Inclusive cycle limit. */
#define CYCLER_MAX_SECONDS 300U /**< Inclusive duration limit; seconds. */
#define CYCLER_ENABLE_TEMPERATURE 0 /**< Optional legacy sensor processing. */
#define CYCLER_DEFAULT_COUNT 0U /**< Boot idle until settings are confirmed. */
#define CYCLER_DEFAULT_ON_SECONDS 3U
#define CYCLER_DEFAULT_OFF_SECONDS 2U
#define CYCLER_RELAY_ACTIVE_HIGH 0 /**< LOW energizes; HIGH releases both relays. */
#define CYCLER_DISPLAY_PHASE 0U /**< ADE phase A=0, B=1, C=2. */
#define CYCLER_V_DECIMALS 1U /**< Five-character numerical voltage field. */
#define CYCLER_A_DECIMALS 1U /**< Five-character numerical current field. */
#define CYCLER_P_DECIMALS 0U /**< Four-character numerical watt field. */
#define CYCLER_PF_DECIMALS 2U /**< Four-character PF magnitude field. */
#if CYCLER_DISPLAY_PHASE > 2
#error Invalid ADE display phase
#endif
#if CYCLER_V_DECIMALS > 4 || CYCLER_A_DECIMALS > 4 || CYCLER_P_DECIMALS > 4 || CYCLER_PF_DECIMALS > 4
#error Display decimals must be between zero and four
#endif
#endif
