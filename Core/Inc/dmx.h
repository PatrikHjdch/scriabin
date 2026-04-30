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
typedef enum dmxState_t {
	OFF_STATE,
	BREAK_STATE,
	MAB_STATE,
	TX_STATE
} dmxState_t;

#endif /* INC_DMX_H_ */

// zpristupneni funkci
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart);

void dmxInit(UART_HandleTypeDef* uartInstance, TIM_HandleTypeDef* timInstance, uint16_t nChannels, uint8_t startCode);
void dmxZero();
void dmxBegin();
void dmxOff();
void dmxOn();
void dmxWrite(uint16_t pos, uint8_t value);
dmxState_t dmxGetState();
void checkForDeferredDmxTransition();
void dmxTimerElapsedCallback();
