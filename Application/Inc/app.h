/**
 * @file app.h
 * @brief Stable application entry points called by CubeMX-generated main.c.
 *
 * Only this tiny interface is referenced from generated code. Product features
 * belong in Application/Src or reusable libraries, never inside generated
 * initialization functions or interrupt handlers.
 */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the current application after all MX_* functions finish.
 *
 * The clean baseline starts only the diagnostic LED service. Future features
 * should be initialized here after their CubeMX peripheral handles exist.
 */
void App_Init(void);

/**
 * @brief Execute one non-blocking application iteration.
 *
 * This function must return quickly and be called continuously from while(1).
 * Long transfers should be split into state-machine steps or deferred jobs.
 */
void App_Process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
