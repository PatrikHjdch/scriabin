/*
 * dmx.h
 *
 *  Created on: Jun 22, 2025
 *      Author: Patrik
 */
#include "main.h"
#include <stdint.h>
#ifndef INC_DMX_H_
#define INC_DMX_H_

// doby trvani sekvenci
#define BREAK_DURATION 176
#define MAB_DURATION 12

// stavy
#define OFF_STATE 0
#define BREAK_STATE 1
#define MAB_STATE 2
#define TX_STATE 3

#endif /* INC_DMX_H_ */

// zpristupneni funkci
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim);

void dmxInit(UART_HandleTypeDef* uartInstance, TIM_HandleTypeDef* timInstance, uint16_t nChannels, uint8_t startCode);
void dmxBegin();
void dmxOff();
void dmxOn();
void dmxWrite(uint16_t pos, uint8_t value);
