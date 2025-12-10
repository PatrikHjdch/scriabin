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
#include "usbd_cdc_if.h"
#include "usb_device.h"
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
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */
extern USBD_CDC_HandleTypeDef husb1;
extern uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
extern uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

volatile uint8_t mem1[1048]; // tabulka adres
volatile uint8_t mem2[1024]; // seznam vazeb

// fronta pro timeouty (mozna bude lepsi pouzit struct array)
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
static void MX_I2C1_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM16_Init(void);
/* USER CODE BEGIN PFP */
void scheduleTimeout(uint8_t midiChannel, uint8_t pitch, uint16_t dmxChannel, uint8_t dmxValue, uint16_t timeout); // naplanovani timeoutu
void disableTimeout(uint8_t midiChannel, uint8_t pitch); // vypnuti timeoutu kdyz vcas prijde midi zprava
void checkForTimeouts(); // kontrolovani timeoutu
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void initializePrototypeTable() {
	// ukazatele na pozice v pametech kam zapisujeme
	uint16_t mem1WP = CONFIGS; // rezervujeme prvnich par mist v pameti pro jina nastaveni
	uint16_t mem2WP = 0;

	// vytvarime vazby na kanale 1 pro Note On + Off s cislem noty 60 (C4)
	uint8_t channel = 0;
	uint8_t type = NOTE_TYPE;
	uint8_t pitch = 60;
	uint16_t address = CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + 2 * N_PITCHES * type;
	while (mem1WP <= address) { // vyplneni vsech mezipozic stejnou adresou
		mem1[mem1WP++] = mem2WP >> 8;
		mem1[mem1WP++] = mem2WP & 0xFF;
	}
	mem2[mem2WP++] = 0; // MSB DMX kanalu
	mem2[mem2WP++] = 2; // LSB DMX kanalu
	mem2[mem2WP++] = 0; // nepouzivame velocity
	mem2[mem2WP++] = 255; // DMX hodnota pro Note On
	mem2[mem2WP++] = 0; // DMX hodnota pro Note Off
	mem2[mem2WP++] = 5000 >> 8; // MSB timeoutu
	mem2[mem2WP++] = 5000 & 0xFF; // LSB timeoutu

	pitch = 62; // cislo noty 62 (D4)
	address = CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + 2 * N_PITCHES * type;
	while (mem1WP <= address) { // vyplneni vsech mezipozic stejnou adresou
		mem1[mem1WP++] = mem2WP >> 8;
		mem1[mem1WP++] = mem2WP & 0xFF;
	}
	mem2[mem2WP++] = 0;
	mem2[mem2WP++] = 3;
	mem2[mem2WP++] = 0;
	mem2[mem2WP++] = 255;
	mem2[mem2WP++] = 0;
	mem2[mem2WP++] = 5000 >> 8;
	mem2[mem2WP++] = 5000 & 0xFF;

	pitch = 64; // cislo noty 62 (E4)
	address = CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + 2 * N_PITCHES * type;
	while (mem1WP <= address) {
		mem1[mem1WP++] = mem2WP >> 8;
		mem1[mem1WP++] = mem2WP & 0xFF;
	}
	mem2[mem2WP++] = 0;
	mem2[mem2WP++] = 4;
	mem2[mem2WP++] = 0;
	mem2[mem2WP++] = 255;
	mem2[mem2WP++] = 0;
	mem2[mem2WP++] = 5000 >> 8;
	mem2[mem2WP++] = 5000 & 0xFF;

	while (mem1WP < 1046) { // vyplneni zbytku pameti stejnou adresou
		mem1[mem1WP++] = mem2WP >> 8;
		mem1[mem1WP++] = mem2WP & 0xFF;
	}
}


// USB VBUS Sensing
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
	if (GPIO_Pin == VBUS_DETECT_Pin) {
		if (HAL_GPIO_ReadPin(VBUS_DETECT_GPIO_Port, VBUS_DETECT_Pin) == GPIO_PIN_SET) {
			MX_USB_DEVICE_Init();
		}
		else {
			USB_Device_DeInit();
		}
	}
}

// MIDI handlery
void noteOn(uint8_t channel, uint8_t pitch, uint8_t velocity) { // note on
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // indikace zpracovavani zpravy

	uint16_t entries[2]; // 1: adresa prvni vazby midi zpravy, 2: adresa prvni vazby dalsi midi zpravy

	uint8_t dmxData[NOTE_ENTRY_LENGTH];
	// struktura vazby note on + off:
	// 0: MSB DMX kanalu
	// 1: LSB DMX kanalu
	// 2: pouzivani velocity
	// 3: DMX hodnota pro Note On
	// 4: DMX hodnota pro Note Off
	// 5: timeout MSB
	// 6: timeout LSB

	// struktura vazby note on:
	// 0: MSB DMX kanalu
	// 1: LSB DMX kanalu
	// 2: pouzivani velocity
	// 3: DMX hodnota

	// vytazeni adres z tabulky adres (note on + off)
	uint16_t address = CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + 2 * N_PITCHES * NOTE_TYPE;
	entries[0] = (uint16_t)((mem1[address] << 8) + mem1[address + 1]);
	entries[1] = (uint16_t)((mem1[address + 2] << 8) + mem1[address + 3]);

	for (uint16_t i = entries[0]; i < entries[1]; i = i + NOTE_ENTRY_LENGTH) { // prochazeni celeho seznamu
		memcpy(dmxData, (const void *)&mem2[i], NOTE_ENTRY_LENGTH); // kopirovani dat do nove pracovni pameti
		dmxWrite((dmxData[0] << 8) + dmxData[1], dmxData[2] == 1 ? velocity*2 : dmxData[3]); // zapis do DMX
		if ((dmxData[5] << 8) + dmxData[6] > 0) { // naplanovani timeoutu
			scheduleTimeout(channel, pitch, (dmxData[0] << 8) + dmxData[1], dmxData[4], (dmxData[5] << 8) + dmxData[6]);
		}
	}

	// vytazeni adres z tabulky adres (note on)
	address = CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + 2 * N_PITCHES * NOTE_ON_TYPE;
	entries[0] = (uint16_t)((mem1[address] << 8) + mem1[address + 1]);
	entries[1] = (uint16_t)((mem1[address + 2] << 8) + mem1[address + 3]);

	for (uint16_t i = entries[0]; i < entries[1]; i = i + NOTE_ON_ENTRY_LENGTH) { // prochazeni celeho seznamu
		memcpy(dmxData, (const void *)&mem2[i], NOTE_ON_ENTRY_LENGTH); // kopirovani dat do nove pracovni pameti
		dmxWrite((dmxData[0] << 8) + dmxData[1], dmxData[2] == 1 ? velocity*2 : dmxData[3]); // zapis do DMX
	}
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // konec indikace zpracovavani zpravy
}

void noteOff(uint8_t channel, uint8_t pitch, uint8_t velocity) { // note off
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // indikace zpracovavani zpravy
	uint16_t entries[2]; // 1: adresa prvni vazby midi zpravy, 2: adresa prvni vazby dalsi midi zpravy

	uint8_t dmxData[NOTE_ENTRY_LENGTH];
	// struktura vazby note on + off:
	// 0: MSB DMX kanalu
	// 1: LSB DMX kanalu
	// 2: pouzivani velocity
	// 3: DMX hodnota pro Note On
	// 4: DMX hodnota pro Note Off
	// 5: timeout MSB
	// 6: timeout LSB

	// struktura vazby note off:
	// 0: MSB DMX kanalu
	// 1: LSB DMX kanalu
	// 2: DMX hodnota


	// vytazeni adres z tabulky adres (note on + off)
	uint16_t address = CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + 2 * N_PITCHES * NOTE_TYPE;
	entries[0] = (uint16_t)((mem1[address] << 8) + mem1[address + 1]);
	entries[1] = (uint16_t)((mem1[address + 2] << 8) + mem1[address + 3]);

	for (uint16_t i = entries[0]; i < entries[1]; i = i + NOTE_ENTRY_LENGTH) { // prochazeni celeho seznamu
		memcpy(dmxData, (const void *)&mem2[i], NOTE_ENTRY_LENGTH); // kopirovani dat do pracovni pameti
		dmxWrite((dmxData[0] << 8) + dmxData[1], dmxData[4]); // zapis do DMX
		if ((dmxData[5] << 8) + dmxData[6] > 0) { // ruseni timeoutu
			disableTimeout(channel, pitch);
		}
	}

	// vytazani adres z tabulky adres (note off)
	address = CONFIGS + channel * N_TYPES * N_PITCHES * 2 + pitch * 2 + 2 * N_PITCHES * NOTE_OFF_TYPE;
	entries[0] = (uint16_t)((mem1[address] << 8) + mem1[address + 1]);
	entries[1] = (uint16_t)((mem1[address + 2] << 8) + mem1[address + 3]);

	for (uint16_t i = entries[0]; i < entries[1]; i = i + NOTE_OFF_ENTRY_LENGTH) { // prochazeni celeho seznamu
		memcpy(dmxData, (const void *)&mem2[i], NOTE_OFF_ENTRY_LENGTH); // kopirovani dat do pracovni pameti
		dmxWrite((dmxData[0] << 8) + dmxData[1], dmxData[2]); // zapis do DMX
	}
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // konec indikace zpracovavani zpravy
}

void controlChange(uint8_t channel, uint8_t controller, uint8_t value) { // control change
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET); // indikace zpracovavani zpravy
	if (controller > 119) { // channel mode zpravy
		switch (controller) {
		case ALL_SOUND_OFF: // vynulovani vsech kanalu DMX
			for (uint16_t i = 1; i < 513; i++) {
				dmxWrite(i, 0);
			}
			break;
		}
		HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // konec indikace zpracovavani zpravy
		return;
	}
	uint16_t entries[2]; // 1: adresa prvni vazby midi zpravy, 2: adresa prvni vazby dalsi midi zpravy

	uint8_t dmxData[CONTROL_CHANGE_ENTRY_LENGTH];
	// struktura vazby control change:
	// 0: MSB DMX kanalu
	// 1: LSB DMX kanalu

	// vytazeni adres z tabulky adres (control change)
	uint8_t address = CONFIGS + channel * N_TYPES * N_PITCHES * 2 + controller * 2 + 2 * N_PITCHES * CONTROL_CHANGE_TYPE;
	entries[0] = (uint16_t)((mem1[address] << 8) + mem1[address + 1]);
	entries[1] = (uint16_t)((mem1[address + 2] << 8) + mem1[address + 3]);

	for (uint16_t i = entries[0]; i < entries[1]; i = i + CONTROL_CHANGE_ENTRY_LENGTH) { // prochazeni celeho seznamu
		memcpy(dmxData, (const void *)&mem2[i], CONTROL_CHANGE_ENTRY_LENGTH); // kopirovani dat do pracovni pameti
		dmxWrite(dmxData[0], value*2); // zapis do DMX
	}
	HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); // konec indikace zpracovavani zpravy
}

void scheduleTimeout(uint8_t midiChannel, uint8_t pitch, uint16_t dmxChannel, uint8_t dmxValue, uint16_t timeout) {
	timeoutScheduleMidiChannels[timeoutScheduleWritePtr] = midiChannel;
	timeoutScheduleMidiPitches[timeoutScheduleWritePtr] = pitch;
	timeoutScheduleDmxChannels[timeoutScheduleWritePtr] = dmxChannel;
	timeoutScheduleDmxValues[timeoutScheduleWritePtr] = dmxValue;
	timeoutScheduleTimings[timeoutScheduleWritePtr] = HAL_GetTick() + timeout;
	timeoutScheduleWritePtr++;
	timeoutScheduleWritePtr = timeoutScheduleWritePtr % TIMEOUT_SCHEDULE_LENGTH;
}

void disableTimeout(uint8_t midiChannel, uint8_t pitch) {
	for (uint8_t i = 0; i < TIMEOUT_SCHEDULE_LENGTH; i++) {
		if (timeoutScheduleMidiChannels[i] == midiChannel && timeoutScheduleMidiPitches[i] == pitch) {
			timeoutScheduleMidiChannels[i] = UINT8_MAX;
			timeoutScheduleMidiPitches[i] = UINT8_MAX;
			timeoutScheduleTimings[i] = UINT32_MAX;
		}
	}
}

void checkForTimeouts() {
	for (uint8_t i = 0; i < TIMEOUT_SCHEDULE_LENGTH; i++) {
		if (timeoutScheduleTimings[i] <= HAL_GetTick()) {
			dmxWrite(timeoutScheduleDmxChannels[i], timeoutScheduleDmxValues[i]);
			timeoutScheduleMidiChannels[i] = UINT8_MAX;
			timeoutScheduleMidiPitches[i] = UINT8_MAX;
			timeoutScheduleTimings[i] = UINT32_MAX;
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
  MX_I2C1_Init();
  MX_USART3_UART_Init();
  MX_USART1_UART_Init();
  MX_TIM16_Init();
  /* USER CODE BEGIN 2 */
	midiInit(&huart1, &hdma_usart1_rx);
	dmxInit(&huart3, &htim16, 64, 0x00);

	setNoteOnHandler(noteOn);
	setNoteOffHandler(noteOff);
	setControlChangeHandler(controlChange);

	timeoutScheduleWritePtr = 0;
	for (uint8_t i = 0; i < TIMEOUT_SCHEDULE_LENGTH; i++) {
		timeoutScheduleMidiChannels[i] = UINT8_MAX;
		timeoutScheduleMidiPitches[i] = UINT8_MAX;
		timeoutScheduleTimings[i] = UINT32_MAX;
	}

	// tabulka pro ucely testovani
	initializePrototypeTable();

	// kontrola pripojeni usb
	if (HAL_GPIO_ReadPin(VBUS_DETECT_GPIO_Port, VBUS_DETECT_Pin) == GPIO_PIN_SET) {
		MX_USB_DEVICE_Init();
	}

	dmxWrite(1, 255);
	dmxOn();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	for (;;)
	{
		readMidi(); // zpracovanani prijatych midi zprav z fronty
		checkForTimeouts(); // kontrola timeoutu
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
                              |RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_TIM16;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.USBClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
  PeriphClkInit.Tim16ClockSelection = RCC_TIM16CLK_HCLK;
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
  hi2c1.Init.Timing = 0x00201D2B;
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
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_DISABLE) != HAL_OK)
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
  huart3.Init.Mode = UART_MODE_TX;
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

  /*Configure GPIO pin : VBUS_DETECT_Pin */
  GPIO_InitStruct.Pin = VBUS_DETECT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(VBUS_DETECT_GPIO_Port, &GPIO_InitStruct);

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
