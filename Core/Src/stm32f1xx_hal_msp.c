/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f1xx_hal_msp.c
  * @brief   Global MCU Support Package initialization
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"

void HAL_MspInit(void)
{
  __HAL_RCC_AFIO_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();

  /* Keep SWD active while releasing the remaining JTAG pins for future GPIO. */
  __HAL_AFIO_REMAP_SWJ_NOJTAG();
}
