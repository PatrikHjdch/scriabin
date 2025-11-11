/*
 * dmx.c
 *
 *  Created on: Jun 22, 2025
 *      Author: Patrik
 */

#include "dmx.h"

static uint8_t dmxOutput[513];

static uint16_t channels;
static uint8_t outputOn;
static UART_HandleTypeDef* uart;

void usDelay(int delay) {
	uint32_t start = TIM1->CNT;
	uint32_t duration = delay * 48;
	while (TIM1->CNT - start < duration);
}

static void switchDmxPinToGPIO() {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}

static void switchDmxPinToUART() {
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_PIN_10;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
}




void dmxInit(UART_HandleTypeDef* uartInstance, uint16_t nChannels, uint8_t startCode) {
	dmxOutput[0] = startCode;
	for (uint16_t i = 1; i < 513; i++) {
		dmxOutput[i] = 0;
	}
	channels = nChannels;
	uart = uartInstance;
	outputOn = 0;
}


void dmxOff() {
	outputOn = 0;
}

void dmxOn() {
	outputOn = 1;
}

void dmxWrite(uint16_t pos, uint8_t value) {
	dmxOutput[pos] = value;
}

static void sendDmx() {
	switchDmxPinToGPIO();
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);
	usDelay(BREAK_DURATION);
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET);
	usDelay(MAB_DURATION);
	switchDmxPinToUART();
	HAL_UART_Transmit(uart, dmxOutput, channels+1, 100);
}

static void runDmx() {
	for(;;) {
		if(UART_CheckIdleState(uart) == HAL_OK) {
			sendDmx();
		} else {
			usDelay(20);
		}
	}
}

void startDmx() {
	dmxOn();
	runDmx();
}
