/*
 * dmx.c
 *
 *  Created on: Jun 22, 2025
 *      Author: Patrik
 */

#include "dmx.h"

static uint8_t dmxOutput[513]; // drzitel dmx signalu vcetne startovniho kodu na pozici 0

static uint16_t channels; // pocet prenasenych kanalu
static UART_HandleTypeDef* uart; // ukazatel na instanci uart rozhrani
static uint8_t state; // stav automatu
static TIM_HandleTypeDef* tim; // ukazatel na instanci casovace

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) { // po uspesnem prenosu po dma
	if (huart == uart && state == TX_STATE) {
		// prepnuti pinu na GPIO rezim
		GPIOB->MODER &= ~(3u << (10*2));
		GPIOB->MODER |= (1u << (10*2));
		// zapsani 0 na pin
		GPIOB->BSRR = (1u << (10 + 16));
		// zapnuti casovace a nastaveni periody
		tim->Instance->ARR = BREAK_DURATION;
		tim->Instance->CNT = 0;
		tim->Instance->DIER |= TIM_DIER_UIE;
		// zmena stavu
		state = BREAK_STATE;
	}
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim) { // po vyprseni casovace
	if (htim == tim) {
		switch (state) {
		case BREAK_STATE: // po BREAK
			// zapsani 1 na pin
			GPIOB->BSRR = (1u << 10);
			// nastaveni periody
			tim->Instance->ARR = MAB_DURATION;
			// zmena stavu
			state = MAB_STATE;
			break;
		case MAB_STATE: // po MAB
			// zmena rezimu pinu na alternativni funkci (uart)
			GPIOB->MODER &= ~(3u << (10*2));
			GPIOB->MODER |= (2u << (10*2));
			// vypnuti casovace
			tim->Instance->ARR = UINT16_MAX;
			tim->Instance->DIER &= ~TIM_DIER_UIE;
			// zmena stavu
			state = TX_STATE;
			// zacatek prenosu
			HAL_UART_Transmit_DMA(uart, dmxOutput, channels);
			break;
		}
	}
}

void dmxInit(UART_HandleTypeDef* uartInstance, TIM_HandleTypeDef* timInstance, uint16_t nChannels, uint8_t startCode) { // inicializace
	dmxOutput[0] = startCode; // startovni kod je vzdy na zacatku prenosu
	for (uint16_t i = 1; i < 513; i++) {
		dmxOutput[i] = 0; // zaciname s vynulovanym vesmirem
	}
	channels = nChannels; // pocet prenasenych kanalu
	uart = uartInstance; // ukazatel na instanci uart rozhrani
	tim = timInstance; // ukazatel na instanci casovace
	state = OFF_STATE; // zaciname s vypnutym prenosem
}


void dmxOff() {
	tim->Instance->DIER &= ~TIM_DIER_UIE; // vypnuti preruseni casovace
	state = OFF_STATE; // zmena stavu
}

void dmxOn() {
	HAL_TIM_Base_Start_IT(tim); // spusteni casovace s prerusenim
	// zmena pinu na rezim gpio
	GPIOB->MODER &= ~(3u << (10*2));
	GPIOB->MODER |= (1u << (10*2));
	// zapsani 0 na pin
	GPIOB->BSRR = (1u << (10 + 16));
	// nastaveni casovace a zapnuti preruseni
	tim->Instance->ARR = BREAK_DURATION;
	tim->Instance->CNT = 0;
	tim->Instance->DIER |= TIM_DIER_UIE;
	// zmena stavu
	state = BREAK_STATE;
}

void dmxWrite(uint16_t pos, uint8_t value) { // zapis do DMX signalu
	dmxOutput[pos] = value;
}
