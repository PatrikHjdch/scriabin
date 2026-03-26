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
DMA_HandleTypeDef hdma_usart1_rx;
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
		Error_Handler();
		break;
	case STATE_WORKING:
		dmxOn();
		midiReceiveOn();
		break;
	case STATE_UPLOAD:
		dmxOff();
		midiReceiveOff();
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

// MIDI handlery
//void noteOn(uint8_t channel, uint8_t pitch, uint8_t velocity) { // note on
//	operationResult_t result;
//	//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // indikace zpracovavani zpravy
//
//	uint16_t addresses[2]; // 1: adresa prvni vazby midi zpravy, 2: adresa prvni vazby dalsi midi zpravy
//
//	uint8_t dmxData[NOTE_LINK_DATA_LENGTH];
//	// struktura vazby note on + off:
//	// 0: MSB DMX kanalu
//	// 1: LSB DMX kanalu
//	// 2: pouzivani velocity
//	// 3: DMX hodnota pro Note On
//	// 4: DMX hodnota pro Note Off
//	// 5: timeout MSB
//	// 6: timeout LSB
//
//	// struktura vazby note on:
//	// 0: MSB DMX kanalu
//	// 1: LSB DMX kanalu
//	// 2: pouzivani velocity
//	// 3: DMX hodnota
//
//	// vytazeni adres z tabulky adres (note on + off)
//	result = retrieveLinkAddresses(addresses, activeProfile, channel, NOTE_LINK, pitch);
//	if (result != SC_OK) setState(STATE_BROKEN);
//
//	for (uint16_t i = addresses[0]; i < addresses[1]; i = i + NOTE_LINK_DATA_LENGTH) { // prochazeni celeho seznamu
//		result = retrieveLinkData(dmxData, i, NOTE_LINK_DATA_LENGTH); // kopirovani dat do nove pracovni pameti
//		if (result != SC_OK) setState(STATE_BROKEN);
//		//retrieveLink(dmxData, i, NOTE_LINK_DATA_LENGTH);
//
//		dmxWrite((uint16_t)((dmxData[0] << 8) | dmxData[1]), dmxData[2] == 1 ? velocity*2 : dmxData[3]); // zapis do DMX
//		if ((dmxData[5] << 8) + dmxData[6] > 0) { // naplanovani timeoutu
//			scheduleTimeout(channel, pitch, (dmxData[0] << 8) + dmxData[1], dmxData[4], (dmxData[5] << 8) + dmxData[6]);
//		}
//	}
//
//	// vytazeni adres z tabulky adres (note on)
//	result = retrieveLinkAddresses(addresses, activeProfile, channel, NOTE_ON_LINK, pitch);
//	if (result != SC_OK) setState(STATE_BROKEN);
//
//	for (uint16_t i = addresses[0]; i < addresses[1]; i = i + NOTE_ON_LINK_DATA_LENGTH) { // prochazeni celeho seznamu
//		result = retrieveLinkData(dmxData, i, NOTE_ON_LINK_DATA_LENGTH); // kopirovani dat do nove pracovni pameti
//		if (result != SC_OK) setState(STATE_BROKEN);
//
//		dmxWrite((uint16_t)((dmxData[0] << 8) | dmxData[1]), dmxData[2] == 1 ? velocity*2 : dmxData[3]); // zapis do DMX
//	}
//	//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // konec indikace zpracovavani zpravy
//}
//
//void noteOff(uint8_t channel, uint8_t pitch, uint8_t velocity) { // note off
//	operationResult_t result;
//	//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // indikace zpracovavani zpravy
//	uint16_t addresses[2]; // 1: adresa prvni vazby midi zpravy, 2: adresa prvni vazby dalsi midi zpravy
//
//	uint8_t dmxData[NOTE_LINK_DATA_LENGTH];
//	// struktura vazby note on + off:
//	// 0: MSB DMX kanalu
//	// 1: LSB DMX kanalu
//	// 2: pouzivani velocity
//	// 3: DMX hodnota pro Note On
//	// 4: DMX hodnota pro Note Off
//	// 5: timeout MSB
//	// 6: timeout LSB
//
//	// struktura vazby note off:
//	// 0: MSB DMX kanalu
//	// 1: LSB DMX kanalu
//	// 2: DMX hodnota
//
//
//	// vytazeni adres z tabulky adres (note on + off)
//	result = retrieveLinkAddresses(addresses, activeProfile, channel, NOTE_LINK, pitch);
//	if (result != SC_OK) setState(STATE_BROKEN);
//
//	for (uint16_t i = addresses[0]; i < addresses[1]; i = i + NOTE_LINK_DATA_LENGTH) { // prochazeni celeho seznamu
//		result = retrieveLinkData(dmxData, i, NOTE_LINK_DATA_LENGTH); // kopirovani dat do pracovni pameti
//		if (result != SC_OK) setState(STATE_BROKEN);
//
//		dmxWrite((uint16_t)((dmxData[0] << 8) | dmxData[1]), dmxData[4]); // zapis do DMX
//		if ((dmxData[5] << 8) + dmxData[6] > 0) { // ruseni timeoutu
//			disableTimeout(channel, pitch);
//		}
//	}
//
//	// vytazani adres z tabulky adres (note off)
//	result = retrieveLinkAddresses(addresses, activeProfile, channel, NOTE_OFF_LINK, pitch);
//	if (result != SC_OK) setState(STATE_BROKEN);
//
//	for (uint16_t i = addresses[0]; i < addresses[1]; i = i + NOTE_OFF_LINK_DATA_LENGTH) { // prochazeni celeho seznamu
//		result = retrieveLinkData(dmxData, i, NOTE_OFF_LINK_DATA_LENGTH); // kopirovani dat do pracovni pameti
//		if (result != SC_OK) setState(STATE_BROKEN);
//
//		dmxWrite((uint16_t)((dmxData[0] << 8) | dmxData[1]), dmxData[2]); // zapis do DMX
//	}
//	//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // konec indikace zpracovavani zpravy
//}
//
//void controlChange(uint8_t channel, uint8_t controller, uint8_t value) { // control change
//	operationResult_t result;
//	//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // indikace zpracovavani zpravy
//	if (controller > 119) { // channel mode zpravy
//		switch (controller) {
//		case ALL_SOUND_OFF: // vynulovani vsech kanalu DMX
//			for (uint16_t i = 1; i < 513; i++) {
//				dmxWrite(i, 0);
//			}
//			break;
//		}
//		HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // konec indikace zpracovavani zpravy
//		return;
//	}
//	uint16_t addresses[2]; // 1: adresa prvni vazby midi zpravy, 2: adresa prvni vazby dalsi midi zpravy
//
//	uint8_t dmxData[CONTROL_CHANGE_LINK_DATA_LENGTH];
//	// struktura vazby control change:
//	// 0: MSB DMX kanalu
//	// 1: LSB DMX kanalu
//
//	// vytazeni adres z tabulky adres (control change)
//	result = retrieveLinkAddresses(addresses, activeProfile, channel, CONTROL_CHANGE_LINK, controller);
//	if (result != SC_OK) setState(STATE_BROKEN);
//
//	for (uint16_t i = addresses[0]; i < addresses[1]; i = i + CONTROL_CHANGE_LINK_DATA_LENGTH) { // prochazeni celeho seznamu
//		result = retrieveLinkData(dmxData, i, CONTROL_CHANGE_LINK_DATA_LENGTH); // kopirovani dat do pracovni pameti
//		if (result != SC_OK) setState(STATE_BROKEN);
//
//		dmxWrite((uint16_t)((dmxData[0] << 8) | dmxData[1]), value*2); // zapis do DMX
//	}
//	//HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // konec indikace zpracovavani zpravy
//}


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
	midiInit(&huart1, &hdma_usart1_rx);
	setNoteOnHandler(noteOnAsyncStart);
	setNoteOffHandler(noteOffAsyncStart);
	setControlChangeHandler(controlChangeAsyncStart);
	MX_USB_DEVICE_Init();

	dmxOn();
	midiReceiveOn();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	for (;;)
	{
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
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
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
  HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 12, 0);
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
  HAL_GPIO_WritePin(GPIOC, MEM2_WP_Pin|MEM1_WP_Pin|PROF_2_LED_Pin|USB_LED_Pin
                          |MYI2C_SCL_Pin|MYI2C_SDA_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD2_Pin|PROF_3_LED_Pin|PROF_1_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MEM2_WP_Pin MEM1_WP_Pin PROF_2_LED_Pin USB_LED_Pin */
  GPIO_InitStruct.Pin = MEM2_WP_Pin|MEM1_WP_Pin|PROF_2_LED_Pin|USB_LED_Pin;
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

  /*Configure GPIO pins : PROF_2_BTN_Pin PROF_1_BTN_Pin */
  GPIO_InitStruct.Pin = PROF_2_BTN_Pin|PROF_1_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin PROF_3_LED_Pin PROF_1_LED_Pin */
  GPIO_InitStruct.Pin = LD2_Pin|PROF_3_LED_Pin|PROF_1_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : VBUS_DETECT_Pin */
  GPIO_InitStruct.Pin = VBUS_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(VBUS_DETECT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PROF_3_BTN_Pin */
  GPIO_InitStruct.Pin = PROF_3_BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PROF_3_BTN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : MYI2C_SCL_Pin MYI2C_SDA_Pin */
  GPIO_InitStruct.Pin = MYI2C_SCL_Pin|MYI2C_SDA_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
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
