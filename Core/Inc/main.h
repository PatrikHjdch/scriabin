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
#include "customTypeDef.h"
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
#define B1_EXTI_IRQn EXTI15_10_IRQn
#define MEM2_WP_Pin GPIO_PIN_2
#define MEM2_WP_GPIO_Port GPIOC
#define MEM1_WP_Pin GPIO_PIN_3
#define MEM1_WP_GPIO_Port GPIOC
#define PROF_2_BTN_Pin GPIO_PIN_12
#define PROF_2_BTN_GPIO_Port GPIOB
#define PROF_2_BTN_EXTI_IRQn EXTI15_10_IRQn
#define LD2_Pin GPIO_PIN_13
#define LD2_GPIO_Port GPIOB
#define PROF_1_BTN_Pin GPIO_PIN_15
#define PROF_1_BTN_GPIO_Port GPIOB
#define PROF_1_BTN_EXTI_IRQn EXTI15_10_IRQn
#define PROF_2_LED_Pin GPIO_PIN_7
#define PROF_2_LED_GPIO_Port GPIOC
#define USB_LED_Pin GPIO_PIN_8
#define USB_LED_GPIO_Port GPIOC
#define VBUS_DETECT_Pin GPIO_PIN_9
#define VBUS_DETECT_GPIO_Port GPIOA
#define PROF_3_BTN_Pin GPIO_PIN_10
#define PROF_3_BTN_GPIO_Port GPIOA
#define PROF_3_BTN_EXTI_IRQn EXTI15_10_IRQn
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define MYI2C_SCL_Pin GPIO_PIN_10
#define MYI2C_SCL_GPIO_Port GPIOC
#define MYI2C_SDA_Pin GPIO_PIN_11
#define MYI2C_SDA_GPIO_Port GPIOC
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB
#define PROF_3_LED_Pin GPIO_PIN_5
#define PROF_3_LED_GPIO_Port GPIOB
#define PROF_1_LED_Pin GPIO_PIN_6
#define PROF_1_LED_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

// kontrolery channel mode zprav
#define ALL_SOUND_OFF 120

void usDelay(uint16_t delay);
void noteOnAsyncStart(uint8_t channel, uint8_t pitch, uint8_t velocity);
void noteOffAsyncStart(uint8_t channel, uint8_t pitch, uint8_t velocity);
void controlChangeAsyncStart(uint8_t channel, uint8_t controller, uint8_t value);
void scheduleTimeout(uint8_t midiChannel, uint8_t pitch, uint16_t dmxChannel, uint8_t dmxValue, uint16_t timeout); // naplanovani timeoutu
void disableTimeout(uint8_t midiChannel, uint8_t pitch); // vypnuti timeoutu kdyz vcas prijde midi zprava
void checkForTimeouts(); // kontrolovani timeoutu

void setState(mainState_t state);
mainState_t getState();

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
