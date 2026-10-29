/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
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
#include "stm32f4xx_hal.h"

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
#define preheat_boiler_Pin GPIO_PIN_0
#define preheat_boiler_GPIO_Port GPIOA
#define steam_boiler_Pin GPIO_PIN_1
#define steam_boiler_GPIO_Port GPIOA
#define coffee_boiler_Pin GPIO_PIN_2
#define coffee_boiler_GPIO_Port GPIOA
#define fill_valve_Pin GPIO_PIN_3
#define fill_valve_GPIO_Port GPIOA
#define brew_valve_Pin GPIO_PIN_4
#define brew_valve_GPIO_Port GPIOA
#define hot_water_Pin GPIO_PIN_5
#define hot_water_GPIO_Port GPIOA
#define water_steam_level_Pin GPIO_PIN_6
#define water_steam_level_GPIO_Port GPIOA
#define pwm_water_level_Pin GPIO_PIN_8
#define pwm_water_level_GPIO_Port GPIOA
#define pwm_steam_valve_Pin GPIO_PIN_4
#define pwm_steam_valve_GPIO_Port GPIOB
#define pump_tacho_Pin GPIO_PIN_5
#define pump_tacho_GPIO_Port GPIOB
#define pump_tacho_EXTI_IRQn EXTI9_5_IRQn
#define pwm_pump_Pin GPIO_PIN_6
#define pwm_pump_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
