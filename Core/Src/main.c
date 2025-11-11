/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include <usbComms.h>
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dmx.h"
#include "midi.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define CONFIGS 15
#define MEM1_ADDRESS 0b10100010
#define MEM2_ADDRESS 0b10100000

#define N_CHANNELS 16
#define N_PITCHES 128
#define N_TYPES 4

#define NOTE_LINK_ENTRY_LENGTH 7
#define NOTE_ON_ENTRY_LENGTH 4
#define NOTE_OFF_ENTRY_LENGTH 3
#define CONTROL_CHANGE_ENTRY_LENGTH 2

#define TIMEOUT_SCHEDULE_LENGTH 32
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */
extern USBD_CDC_HandleTypeDef;
uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

uint8_t timeoutScheduleMidiChannels[TIMEOUT_SCHEDULE_LENGTH];
uint8_t timeoutScheduleMidiPitches[TIMEOUT_SCHEDULE_LENGTH];
uint16_t timeoutScheduleDmxChannels[TIMEOUT_SCHEDULE_LENGTH];
uint8_t timeoutScheduleDmxValues[TIMEOUT_SCHEDULE_LENGTH];
uint32_t timeoutScheduleTimings[TIMEOUT_SCHEDULE_LENGTH];
uint8_t timeoutScheduleWritePtr;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// MIDI message handlers
void noteOn(uint8_t channel, uint8_t pitch, uint8_t velocity) {
	uint8_t addresses[4]; // buffer to get the 16bit addresses of the entries:
	uint16_t entries[2]; // 1st value = address of 1st entry, 2nd value = address of 1st entry of next pitch
	// note on + off:
	// bytes 0 & 1 = MSB & LSB of dmx channel, byte 2 = use velocity?, byte 3 = dmx value for note on, byte 4 = dmx value for note off, bytes 5 & 6 = timeout MSB & LSB
	// note on:
	// bytes 0 & 1 = MSB & LSB of dmx channel, byte 2 = use velocity?, byte 3 = dmx value
	uint8_t dmxData[NOTE_LINK_ENTRY_LENGTH];
	HAL_I2C_Mem_Read(hi2c, MEM1_ADDRESS, CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2, I2C_MEMADD_SIZE_16BIT, addresses, 4, 100);
	entries[0] = (uint16_t)(addresses[0] << 8 + addresses[1]);
	entries[1] = (uint16_t)(addresses[2] << 8 + addresses[3]);
	for (uint16_t i = entries[0]; i < entries[1]; i = i + NOTE_LINK_ENTRY_LENGTH) {
		HAL_I2C_Mem_Read(&hi2c1, MEM2_ADDRESS, i, I2C_MEMADD_SIZE_16BIT, dmxData, NOTE_LINK_ENTRY_LENGTH, 100);
		dmxWrite(dmxData[0] << 8 + dmxData[1], dmxData[2] == 1 ? velocity*2 : dmxData[3]);
		if (dmxData[5] << 8 + dmxData[6] > 0) {
			scheduleTimeout(channel, pitch, dmxData[0] << 8 + dmxData[1], dmxData[4], dmxData[5] << 8 + dmxData[6]);
		}
	}

	HAL_I2C_Mem_Read(hi2c, MEM1_ADDRESS, CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + N_PITCHES * 1, I2C_MEMADD_SIZE_16BIT, addresses, 4, 100);
	entries[0] = (uint16_t)(addresses[0] << 8 + addresses[1]);
	entries[1] = (uint16_t)(addresses[2] << 8 + addresses[3]);
	for (uint16_t i = entries[0]; i < entries[1]; i = i + NOTE_ON_ENTRY_LENGTH) {
		HAL_I2C_Mem_Read(&hi2c1, MEM2_ADDRESS, i, I2C_MEMADD_SIZE_16BIT, dmxData, NOTE_ON_ENTRY_LENGTH, 100);
		dmxWrite(dmxData[0] << 8 + dmxData[1], dmxData[2] == 1 ? velocity*2 : dmxData[3]);
	}
}

void noteOff(uint8_t channel, uint8_t pitch, uint8_t velocity) {
	uint8_t addresses[4]; // buffer to get the 16bit addresses of the entries:
	uint16_t entries[2]; // 1st value = address of 1st entry, 2nd value = address of 1st entry of next pitch
	uint8_t dmxData[NOTE_LINK_ENTRY_LENGTH]; // bytes 1 & 2 = MSB & LSB of dmx channel, byte 3 = dmx value

	HAL_I2C_Mem_Read(hi2c, MEM1_ADDRESS, CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2, I2C_MEMADD_SIZE_16BIT, addresses, 4, 100);
		entries[0] = (uint16_t)(addresses[0] << 8 + addresses[1]);
		entries[1] = (uint16_t)(addresses[2] << 8 + addresses[3]);
		for (uint16_t i = entries[0]; i < entries[1]; i = i + NOTE_LINK_ENTRY_LENGTH) {
			HAL_I2C_Mem_Read(&hi2c1, MEM2_ADDRESS, i, I2C_MEMADD_SIZE_16BIT, dmxData, NOTE_LINK_ENTRY_LENGTH, 100);
			dmxWrite(dmxData[0] << 8 + dmxData[1], dmxData[4]);
		}
		if (dmxData[5] << 8 + dmxData[6] > 0) {
			disableTimeout(channel, pitch);
		}

	HAL_I2C_Mem_Read(hi2c, MEM1_ADDRESS, CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + N_PITCHES * 2, I2C_MEMADD_SIZE_16BIT, addresses, 4, 100);
	entries[0] = (uint16_t)(addresses[0] << 8 + addresses[1]);
	entries[1] = (uint16_t)(addresses[2] << 8 + addresses[3]);
	for (uint16_t i = entries[0]; i < entries[1]; i = i + NOTE_OFF_ENTRY_LENGTH) {
		HAL_I2C_Mem_Read(&hi2c1, MEM2_ADDRESS, i, I2C_MEMADD_SIZE_16BIT, dmxData, NOTE_OFF_ENTRY_LENGTH, 100);
		dmxWrite(dmxData[0] << 8 + dmxData[1], dmxData[2]);
	}
}

void controlChange(uint8_t channel, uint8_t controller, uint8_t value) {
	uint8_t addresses[4]; // buffer to get the 16bit addresses of the entries:
	uint16_t entries[2]; // 1st value = address of 1st entry, 2nd value = address of 1st entry of next pitch
	uint8_t dmxData[CONTROL_CHANGE_ENTRY_LENGTH]; // dmx channel
	HAL_I2C_Mem_Read(hi2c, MEM1_ADDRESS, CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + 2 * N_PITCHES * 3, I2C_MEMADD_SIZE_16BIT, addresses, 4, 100);
	entries[0] = (uint16_t)(addresses[0] << 8 + addresses[1]);
	entries[1] = (uint16_t)(addresses[2] << 8 + addresses[3]);
	for (uint16_t i = entries[0]; i < entries[1]; i = i + CONTROL_CHANGE_ENTRY_LENGTH) {
		HAL_I2C_Mem_Read(&hi2c1, MEM2_ADDRESS, i, I2C_MEMADD_SIZE_16BIT, dmxData, CONTROL_CHANGE_ENTRY_LENGTH, 100);
		dmxWrite(dmxData[0], value*2);
	}
}

void scheduleTimeout(uint8_t midiChannel, uint8_t pitch, uint16_t dmxChannel, uint8_t dmxValue, uint16_t timeout) {
	timeoutScheduleMidiChannels[timeoutScheduleWritePtr] = midiChannel;
	timeoutScheduleMidiPitches[timeoutScheduleWritePtr] = pitch;
	timeoutScheduleDmxChannels[timeoutScheduleWritePtr] = dmxChannel;
	timeoutScheduleDmxValues[timeoutScheduleWritePtr] = dmxValue;
	timeoutScheduleTimings[timeoutScheduleWritePtr] = HAL_GetTick() + timeout;
	timeoutScheduleWritePtr++;
}

void disableTimeout(uint8_t midiChannel, uint8_t pitch) {
	for (uint8_t i = 0; i < TIMEOUT_SCHEDULE_LENGTH; i++) {
		if (timeoutScheduleMidiChannels[i] == midiChannel && timeoutScheduleMidiPitches[i] = pitch) {
			timeoutScheduleMidiChannels[i] = UINT8_MAX;
			timeoutScheduleMidiPitches[i] = UINT8_MAX;
			timeoutScheduleDmxChannels[i] = UINT16_MAX;
			timeoutScheduleTimings[i] = UINT32_MAX;
		}
	}
}

void checkForTimeouts() {
	for (uint8_t i = 0; i < TIMEOUT_SCHEDULE_LENGTH; i++) {
		if (noteOffQueueTimings[i] <= HAL_GetTick()) {
			noteOff(noteOffQueue[i*2], noteOffQueue[i*2 + 1], 0);
			noteOffQueueTimings[i] = UINT32_MAX;
		}
	}
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART3_UART_Init();
  MX_USART1_UART_Init();
  MX_USB_DEVICE_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
	midiInit(&huart1, &hdma_usart1_rx);
	dmxInit(&huart3, 64, 0x00);

	timeoutScheduleWritePtr = 0;

	setNoteOnHandler(noteOn);
	setNoteOffHandler(noteOff);
	setControlChangeHandler(controlChange);


	for (uint8_t i = 0; i < TIMEOUT_SCHEDULE_LENGTH; i++) {
		timeoutScheduleMidiChannels[i] = UINT8_MAX;
		timeoutScheduleMidiPitches[i] = UINT8_MAX;
		timeoutScheduleDmxChannels[i] = UINT16_MAX;
		timeoutScheduleTimings[i] = UINT32_MAX;
	}
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	for (;;)
	{
		checkForTimeouts();
	}
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_USART1
                              |RCC_PERIPHCLK_I2C1;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x0010020A;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 31250;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 250000;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_2;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_RS485Ex_Init(&huart3, UART_DE_POLARITY_HIGH, 0, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  /* DMA1_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel5_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, MEM2_WP_Pin|MEM1_WP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MEM2_WP_Pin MEM1_WP_Pin */
  GPIO_InitStruct.Pin = MEM2_WP_Pin|MEM1_WP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA2 PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void StartMidiTask(void *argument) {
	for(;;)
	{
		readMidi();
	}
}

void StartDmxTask(void *argument) {
	startDmx();
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
