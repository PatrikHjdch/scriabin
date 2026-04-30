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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "dmx.h"
#include "midi.h"
#include "myI2C.h"
#include "usbd_cdc_if.h"
#include "usb_device.h"
#include "usbComms.h"
#include "asyncI2cFetchManager.h"
#include "timeoutManager.h"
#include "asyncUploadManager.h"
#include "mapProperties.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim16;
TIM_HandleTypeDef htim17;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */
static mainState_t mainState;

extern USBD_CDC_HandleTypeDef husb1;

uint8_t activeProfile;

static pinStruct_t profileLedPins[N_PROFILES] = {
		{ PROF_1_LED_GPIO_Port, PROF_1_LED_Pin },
		{ PROF_2_LED_GPIO_Port, PROF_2_LED_Pin },
		{ PROF_3_LED_GPIO_Port, PROF_3_LED_Pin }
};

static uint32_t midiIndicatorTimeout;
static uint32_t dmxIndicatorTimeout;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM16_Init(void);
static void MX_TIM17_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void midiIndicatorOn() {
	midiIndicatorTimeout = HAL_GetTick() + INDICATOR_DURATION;
	HAL_GPIO_WritePin(MIDI_ACTIVITY_LED_GPIO_Port, MIDI_ACTIVITY_LED_Pin, GPIO_PIN_SET);
}

void dmxIndicatorOn() {
	dmxIndicatorTimeout = HAL_GetTick() + INDICATOR_DURATION;
	HAL_GPIO_WritePin(DMX_ACTIVITY_LED_GPIO_Port, DMX_ACTIVITY_LED_Pin, GPIO_PIN_SET);
}

static void indicatorsTimeout() {
	if (HAL_GetTick() >= midiIndicatorTimeout) HAL_GPIO_WritePin(MIDI_ACTIVITY_LED_GPIO_Port, MIDI_ACTIVITY_LED_Pin, GPIO_PIN_RESET);
	if (HAL_GetTick() >= dmxIndicatorTimeout) HAL_GPIO_WritePin(DMX_ACTIVITY_LED_GPIO_Port, DMX_ACTIVITY_LED_Pin, GPIO_PIN_RESET);
}

void setProfile(uint8_t p) {
	activeProfile = p;
	for (uint8_t i = 0; i < N_PROFILES; i++) {
		HAL_GPIO_WritePin(profileLedPins[i].port, profileLedPins[i].pin, p == i ? GPIO_PIN_SET : GPIO_PIN_RESET);
	}
	USB_TX_Profile_Event(p);
}

void setState(mainState_t state) {
	if (mainState == state) return;
	mainState = state;
	switch (mainState) {
	case STATE_BROKEN:
		dmxOff();
		midiReceiveOff();
		HAL_GPIO_WritePin(STAT_LED_R_GPIO_Port, STAT_LED_R_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(STAT_LED_G_GPIO_Port, STAT_LED_G_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(STAT_LED_B_GPIO_Port, STAT_LED_B_Pin, GPIO_PIN_SET);
		Error_Handler();
		break;
	case STATE_WORKING:
		HAL_GPIO_WritePin(STAT_LED_R_GPIO_Port, STAT_LED_R_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(STAT_LED_G_GPIO_Port, STAT_LED_G_Pin, GPIO_PIN_RESET);
		HAL_GPIO_WritePin(STAT_LED_B_GPIO_Port, STAT_LED_B_Pin, GPIO_PIN_SET);
		dmxOn();
		midiReceiveOn();
		break;
	case STATE_UPLOAD:
		dmxOff();
		midiReceiveOff();
		HAL_GPIO_WritePin(STAT_LED_R_GPIO_Port, STAT_LED_R_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(STAT_LED_G_GPIO_Port, STAT_LED_G_Pin, GPIO_PIN_SET);
		HAL_GPIO_WritePin(STAT_LED_B_GPIO_Port, STAT_LED_B_Pin, GPIO_PIN_RESET);
		break;
	}
}

mainState_t getState() {
	return mainState;
}

void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
	switch (mainState) {
	case STATE_UPLOAD:
		uploader_MemRxCpltCallback(hi2c->State == HAL_I2C_STATE_READY ? I2C_OK : I2C_ERR);
		break;
	case STATE_WORKING:
		midiHandler_MemRxCpltCallback(hi2c->State == HAL_I2C_STATE_READY ? I2C_OK : I2C_ERR);
		break;
	default:
		break;
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) { // po vyprseni casovace
	if (htim == &htim17) myI2C_TimerElapsedCallback();
	if (htim == &htim16) dmxTimerElapsedCallback();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	switch (GPIO_Pin) {
	case KILLSWITCH_Pin:
		dmxZero();
		break;
	case PROF_1_BTN_Pin:
		setProfile(0);
		break;
	case PROF_2_BTN_Pin:
		setProfile(1);
		break;
	case PROF_3_BTN_Pin:
		setProfile(2);
	}
}

void noteOnAsyncStart(uint8_t channel, uint8_t pitch, uint8_t velocity) {
	enqueueMessage(activeProfile, NOTE_ON, channel, pitch, velocity);
}

void noteOffAsyncStart(uint8_t channel, uint8_t pitch, uint8_t velocity) {
	enqueueMessage(activeProfile, NOTE_OFF, channel, pitch, velocity);
}

void controlChangeAsyncStart(uint8_t channel, uint8_t controller, uint8_t value) {
	enqueueMessage(activeProfile, CONTROL_CHANGE, channel, controller, value);
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
  MX_TIM16_Init();
  MX_TIM17_Init();
  /* USER CODE BEGIN 2 */
	//i2cComms_init(&hi2c1);
	timeoutManagerInit();
	setProfile(0);
	mainState = STATE_WORKING;
	pinStruct_t myI2C_sdaStruct = {
		.port = MYI2C_SDA_GPIO_Port,
		.pin = MYI2C_SDA_Pin
	};
	pinStruct_t myI2C_sclStruct = {
		.port = MYI2C_SCL_GPIO_Port,
		.pin = MYI2C_SCL_Pin
	};

	myI2C_Init(myI2C_sdaStruct, myI2C_sclStruct, &htim17);
	HAL_TIM_Base_Start(&htim17);
	dmxInit(&huart3, &htim16, 64, 0x00);
#ifdef USING_IT_MIDI
	midiInit(&huart1);
#elif defined USING_DMA_MIDI
	midiInit(&huart1, &hdma_usart1_rx);
#endif
	setNoteOnHandler(noteOnAsyncStart);
	setNoteOffHandler(noteOffAsyncStart);
	setControlChangeHandler(controlChangeAsyncStart);
	MX_USB_DEVICE_Init();

	HAL_GPIO_WritePin(STAT_LED_R_GPIO_Port, STAT_LED_R_Pin, GPIO_PIN_SET);
	HAL_GPIO_WritePin(STAT_LED_G_GPIO_Port, STAT_LED_G_Pin, GPIO_PIN_RESET);
	HAL_GPIO_WritePin(STAT_LED_B_GPIO_Port, STAT_LED_B_Pin, GPIO_PIN_SET);
	midiReceiveOn();
	dmxOn();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	for (;;)
	{
		indicatorsTimeout();
		if (USB_hasUnhandledData()) USB_HandleIncoming();
		switch (mainState) {
		case STATE_WORKING:
			readMidi(); // parsovani midi zprav
			handleMidiRequests(); // zpracovavani midi requestu
			checkForTimeouts(); // kontrola timeoutu
			break;
		case STATE_UPLOAD:
			mapUploadCheckUpdate();
			break;
		case STATE_BROKEN:
			break;
		}
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB|RCC_PERIPHCLK_USART1
                              |RCC_PERIPHCLK_TIM16|RCC_PERIPHCLK_TIM17;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  PeriphClkInit.Tim16ClockSelection = RCC_TIM16CLK_HCLK;
  PeriphClkInit.Tim17ClockSelection = RCC_TIM17CLK_HCLK;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM16 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM16_Init(void)
{

  /* USER CODE BEGIN TIM16_Init 0 */

  /* USER CODE END TIM16_Init 0 */

  /* USER CODE BEGIN TIM16_Init 1 */

  /* USER CODE END TIM16_Init 1 */
  htim16.Instance = TIM16;
  htim16.Init.Prescaler = 71;
  htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim16.Init.Period = 65535;
  htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim16.Init.RepetitionCounter = 0;
  htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM16_Init 2 */

  /* USER CODE END TIM16_Init 2 */

}

/**
  * @brief TIM17 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM17_Init(void)
{

  /* USER CODE BEGIN TIM17_Init 0 */

  /* USER CODE END TIM17_Init 0 */

  /* USER CODE BEGIN TIM17_Init 1 */

  /* USER CODE END TIM17_Init 1 */
  htim17.Instance = TIM17;
  htim17.Init.Prescaler = 71;
  htim17.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim17.Init.Period = 65535;
  htim17.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim17.Init.RepetitionCounter = 0;
  htim17.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim17) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM17_Init 2 */

  /* USER CODE END TIM17_Init 2 */

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
  huart1.Init.Mode = UART_MODE_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_8;
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
  huart3.Init.Mode = UART_MODE_TX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_8;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
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
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 12, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);

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
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, STAT_LED_R_Pin|STAT_LED_G_Pin|STAT_LED_B_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, MIDI_ACTIVITY_LED_Pin|DMX_ACTIVITY_LED_Pin|USB_LED_Pin|PROF_1_LED_Pin
                          |MYI2C_SCL_Pin|MYI2C_SDA_Pin|MEM1_WP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, PROF_2_LED_Pin|PROF_3_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MEM2_WP_GPIO_Port, MEM2_WP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MEM3_WP_GPIO_Port, MEM3_WP_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA2 PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : STAT_LED_R_Pin STAT_LED_G_Pin STAT_LED_B_Pin MEM3_WP_Pin */
  GPIO_InitStruct.Pin = STAT_LED_R_Pin|STAT_LED_G_Pin|STAT_LED_B_Pin|MEM3_WP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : KILLSWITCH_Pin PROF_1_BTN_Pin PROF_2_BTN_Pin PROF_3_BTN_Pin
                           VIN_DETECT_Pin */
  GPIO_InitStruct.Pin = KILLSWITCH_Pin|PROF_1_BTN_Pin|PROF_2_BTN_Pin|PROF_3_BTN_Pin
                          |VIN_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : MIDI_ACTIVITY_LED_Pin DMX_ACTIVITY_LED_Pin USB_LED_Pin PROF_1_LED_Pin
                           MEM1_WP_Pin */
  GPIO_InitStruct.Pin = MIDI_ACTIVITY_LED_Pin|DMX_ACTIVITY_LED_Pin|USB_LED_Pin|PROF_1_LED_Pin
                          |MEM1_WP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PROF_2_LED_Pin PROF_3_LED_Pin */
  GPIO_InitStruct.Pin = PROF_2_LED_Pin|PROF_3_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : VBUS_DETECT_Pin */
  GPIO_InitStruct.Pin = VBUS_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(VBUS_DETECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MYI2C_SCL_Pin MYI2C_SDA_Pin */
  GPIO_InitStruct.Pin = MYI2C_SCL_Pin|MYI2C_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : MEM2_WP_Pin */
  GPIO_InitStruct.Pin = MEM2_WP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MEM2_WP_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
