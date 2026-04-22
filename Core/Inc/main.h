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
#include "stm32f0xx_hal.h"

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
#define Mode_Switch_Pin GPIO_PIN_1
#define Mode_Switch_GPIO_Port GPIOF
#define Mode_Pin GPIO_PIN_0
#define Mode_GPIO_Port GPIOC
#define ScrollLock_Pin GPIO_PIN_1
#define ScrollLock_GPIO_Port GPIOC
#define CapsLock_Pin GPIO_PIN_2
#define CapsLock_GPIO_Port GPIOC
#define NumLock_Pin GPIO_PIN_3
#define NumLock_GPIO_Port GPIOC
#define Column_13_Pin GPIO_PIN_6
#define Column_13_GPIO_Port GPIOA
#define Row_00_Pin GPIO_PIN_7
#define Row_00_GPIO_Port GPIOA
#define Row_01_Pin GPIO_PIN_4
#define Row_01_GPIO_Port GPIOC
#define Row_02_Pin GPIO_PIN_5
#define Row_02_GPIO_Port GPIOC
#define Row_03_Pin GPIO_PIN_0
#define Row_03_GPIO_Port GPIOB
#define Row_04_Pin GPIO_PIN_1
#define Row_04_GPIO_Port GPIOB
#define Row_05_Pin GPIO_PIN_2
#define Row_05_GPIO_Port GPIOB
#define Row_06_Pin GPIO_PIN_10
#define Row_06_GPIO_Port GPIOB
#define Column_12_Pin GPIO_PIN_11
#define Column_12_GPIO_Port GPIOB
#define OLED_RST_Pin GPIO_PIN_12
#define OLED_RST_GPIO_Port GPIOB
#define Column_11_Pin GPIO_PIN_15
#define Column_11_GPIO_Port GPIOA
#define Column_10_Pin GPIO_PIN_10
#define Column_10_GPIO_Port GPIOC
#define Column_09_Pin GPIO_PIN_11
#define Column_09_GPIO_Port GPIOC
#define Column_08_Pin GPIO_PIN_12
#define Column_08_GPIO_Port GPIOC
#define Column_07_Pin GPIO_PIN_2
#define Column_07_GPIO_Port GPIOD
#define Cplumn_06_Pin GPIO_PIN_3
#define Cplumn_06_GPIO_Port GPIOB
#define Column_05_Pin GPIO_PIN_4
#define Column_05_GPIO_Port GPIOB
#define Column_04_Pin GPIO_PIN_5
#define Column_04_GPIO_Port GPIOB
#define Column_03_Pin GPIO_PIN_6
#define Column_03_GPIO_Port GPIOB
#define Column_02_Pin GPIO_PIN_7
#define Column_02_GPIO_Port GPIOB
#define Column_01_Pin GPIO_PIN_8
#define Column_01_GPIO_Port GPIOB
#define Column_00_Pin GPIO_PIN_9
#define Column_00_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
