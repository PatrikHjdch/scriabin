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

#define BREAK_DURATION 176
#define MAB_DURATION 12

#endif /* INC_DMX_H_ */

void dmxInit(UART_HandleTypeDef* uartInstance, uint16_t nChannels, uint8_t startCode);
void dmxBegin();
void dmxOff();
void dmxOn();
void dmxWrite(uint16_t pos, uint8_t value);
void startDmx();
