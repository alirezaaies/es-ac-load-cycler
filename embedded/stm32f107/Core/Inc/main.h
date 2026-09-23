/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define DEBUG_LED_1_Pin GPIO_PIN_13
#define DEBUG_LED_1_GPIO_Port GPIOC
#define DEBUG_LED_2_Pin GPIO_PIN_14
#define DEBUG_LED_2_GPIO_Port GPIOC
#define ADE_CS_Pin GPIO_PIN_12
#define ADE_CS_GPIO_Port GPIOB
#define ONE_WIRE_1_Pin GPIO_PIN_10
#define ONE_WIRE_1_GPIO_Port GPIOB
#define ONE_WIRE_2_Pin GPIO_PIN_7
#define ONE_WIRE_2_GPIO_Port GPIOC
#define LCD_E_Pin GPIO_PIN_6
#define LCD_E_GPIO_Port GPIOA
#define LCD_RW_Pin GPIO_PIN_7
#define LCD_RW_GPIO_Port GPIOA
#define LCD_RS_Pin GPIO_PIN_8
#define LCD_RS_GPIO_Port GPIOA
#define LCD_D4_Pin GPIO_PIN_9
#define LCD_D4_GPIO_Port GPIOA
#define LCD_D5_Pin GPIO_PIN_10
#define LCD_D5_GPIO_Port GPIOA
#define LCD_D6_Pin GPIO_PIN_11
#define LCD_D6_GPIO_Port GPIOA
#define LCD_D7_Pin GPIO_PIN_12
#define LCD_D7_GPIO_Port GPIOA
#define BUTTON_INC_Pin GPIO_PIN_9
#define BUTTON_INC_GPIO_Port GPIOD
#define BUTTON_INC_EXTI_IRQn EXTI9_5_IRQn
#define BUTTON_DEC_Pin GPIO_PIN_10
#define BUTTON_DEC_GPIO_Port GPIOD
#define BUTTON_DEC_EXTI_IRQn EXTI15_10_IRQn
#define BUTTON_RESET_Pin GPIO_PIN_11
#define BUTTON_RESET_GPIO_Port GPIOD
#define BUTTON_RESET_EXTI_IRQn EXTI15_10_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
