/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "stm32f3xx_hal.h"

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
void usDelay(uint16_t delay);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define MEM2_WP_Pin GPIO_PIN_2
#define MEM2_WP_GPIO_Port GPIOC
#define MEM1_WP_Pin GPIO_PIN_3
#define MEM1_WP_GPIO_Port GPIOC
#define LD2_Pin GPIO_PIN_13
#define LD2_GPIO_Port GPIOB
#define VBUS_DETECT_Pin GPIO_PIN_9
#define VBUS_DETECT_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
// I2C adresy
#define MEM1_ADDRESS 0b10100010
#define MEM2_ADDRESS 0b10100000

// adresy dalsich dat
#define MEM_VALID_ADDRESS 0 // zatim nepouzite - bude se pouzivat k verifikaci uspesneho zapsani dat (na zacatku prepisu se prepne na 0, na konci zase na 1)
#define CONFIGS 15 // delka useku pameti rezervovaneho pro dalsi nastaveni

#define N_CHANNELS 1 // pocet kanalu (z duvodu omezene pameti je v prototypu pouze 1 - po prechodu na eeprom bude vsech 16)
#define N_PITCHES 128 // pocet cisel not
#define N_TYPES 4 // pocet typu

// delky typu vazeb
#define NOTE_ENTRY_LENGTH 7
#define NOTE_ON_ENTRY_LENGTH 4
#define NOTE_OFF_ENTRY_LENGTH 3
#define CONTROL_CHANGE_ENTRY_LENGTH 2

// ciselne hodnoty typu vazeb
#define NOTE_TYPE 0
#define NOTE_ON_TYPE 1
#define NOTE_OFF_TYPE 2
#define CONTROL_CHANGE_TYPE 3

#define TIMEOUT_SCHEDULE_LENGTH 32 // delka fronty pro timeouty

// zpristupneni pameti
extern volatile uint8_t mem1[1048];
extern volatile uint8_t mem2[1024];

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
