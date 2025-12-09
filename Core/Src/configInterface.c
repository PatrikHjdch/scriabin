/*
 * configInterface.c
 *
 *  Created on: Nov 7, 2025
 *      Author: Patrik
 */

#include "configInterface.h"
// ukazatele pozic zapisu do pameti
static uint16_t mem1WritePtr;
static uint16_t mem2WritePtr;

// midi informace
static uint8_t channel;
static uint8_t type;
static uint8_t pitch;

static I2C_HandleTypeDef* i2c; // ukazatel na instanci i2c rozhrani (zatim nevyuzite - pro EEPROM)

uint8_t usbMessageBuf[256]; // buffer prichozich USB zprav
uint32_t usbMessageBufLen; // delka USB zpravy

void configInterfaceInit(I2C_HandleTypeDef* i2cHandle) { // inicializace
	i2c = i2cHandle;
}


static void writeToMem1(uint16_t address, uint16_t value) { // zapis do tabulky adres
	mem1[address] = value >> 8;
	mem1[address + 1] = value & 0xFF;
}

void handleUsb() { // zpracovani USB zpravy
	uint8_t reply[REPLY_LENGTH]; // buffer pro odpoved
	switch (usbMessageBuf[0]) {
	case HELLO: // testovaci zprava
		reply[0] = OUT_HELLO;
		CDC_Transmit_FS(reply, 1);
		break;
	case START_UPLOAD: // zacatek prenosu
		dmxOff(); // vypnuti DMX
		midiReceiveOff(); // vypnuti MIDI
		mem1[MEM_VALID_ADDRESS] = 0; // oznaceni pameti jako neplatne (zatim nepouzivane, bude existovat pro pripad neuspesneho preneseni, po uspenem preneseni se prepise na 1)
		mem1WritePtr = CONFIGS; // rezervujeme prvnich par mist v pameti pro jina nastaveni
		mem2WritePtr = 0;
		channel = 0;
		type = 0;
		pitch = 0;
		reply[0] = OUT_ACK; // vracime acknowledge
		CDC_Transmit_FS(reply, 1);
		break;
	case END_UPLOAD: // konec prenosu
		for (uint16_t i = mem1WritePtr; i < N_ADDRESSES; i = i+2) { // vyplneni zbytku pameti stejnou adresou
			writeToMem1(i, mem2WritePtr);
		}
		reply[0] = OUT_ACK;
		CDC_Transmit_FS(reply, 1);
		mem1[MEM_VALID_ADDRESS] = 1; // oznaceni pameti jako platne
		dmxOn(); // zapnuti DMX
		midiReceiveOn(); // zapnuti MIDI
		break;
	case CHANNEL: // zmena kanalu
		channel = usbMessageBuf[1];
		type = 0;
		pitch = 0;
		if (mem1WritePtr > CONFIGS + channel * N_TYPES * N_PITCHES * 2 + type * N_PITCHES * 2 + pitch * 2) { // pokud prijdou vazby ve spatnem poradi
			reply[0] = OUT_ERROR;
			reply[1] = OUT_ERROR_LINKS_OUT_OF_ORDER;
			CDC_Transmit_FS(reply, 2);
			Error_Handler();
			return;
		}
		while (mem1WritePtr <= CONFIGS + channel * N_TYPES * N_PITCHES * 2 + type * N_PITCHES * 2 + pitch * 2) { // zapsani stejne adresy do vsech mezipozic
			writeToMem1(mem1WritePtr, mem2WritePtr);
			mem1WritePtr = mem1WritePtr + 2;
		}
		reply[0] = OUT_ACK;
		CDC_Transmit_FS(reply, 1);
		break;
	case TYPE: // zmena typu vazby
		type = usbMessageBuf[1];
		pitch = 0;
		if (mem1WritePtr > CONFIGS + channel * N_TYPES * N_PITCHES * 2 + type * N_PITCHES * 2 + pitch * 2) { // pokud prijdou vazby ve spatnem poradi
			reply[0] = OUT_ERROR;
			reply[1] = OUT_ERROR_LINKS_OUT_OF_ORDER;
			CDC_Transmit_FS(reply, 2);
			Error_Handler();
			return;
		}
		while (mem1WritePtr <= CONFIGS + channel * N_TYPES * N_PITCHES * 2 + type * N_PITCHES * 2 + pitch * 2) { // zapsani stejne adresy do vsech mezipozic
			writeToMem1(mem1WritePtr, mem2WritePtr);
			mem1WritePtr = mem1WritePtr + 2;
		}
		reply[0] = OUT_ACK;
		CDC_Transmit_FS(reply, 1);
		break;
	case PITCH: // zmena cisla noty
		pitch = usbMessageBuf[1];
		if (mem1WritePtr > CONFIGS + channel * N_TYPES * N_PITCHES * 2 + type * N_PITCHES * 2 + pitch * 2) { // pokud prijdou vazby ve spatnem poradi
			reply[0] = OUT_ERROR;
			reply[1] = OUT_ERROR_LINKS_OUT_OF_ORDER;
			CDC_Transmit_FS(reply, 2);
			Error_Handler();
			return;
		}
		while (mem1WritePtr <= CONFIGS + channel * N_TYPES * N_PITCHES * 2 + type * N_PITCHES * 2 + pitch * 2) { // zapsani stejne adresy do vsech mezipozic
			writeToMem1(mem1WritePtr, mem2WritePtr);
			mem1WritePtr = mem1WritePtr + 2;
		}
		reply[0] = OUT_ACK;
		CDC_Transmit_FS(reply, 1);
		break;
	case NEW_LINK: // nova vazba
		uint8_t dataLength;
		switch(type) {
		case TYPE_NOTE_LINK:
			dataLength = NOTE_ENTRY_LENGTH;
			break;
		case TYPE_NOTE_ON_LINK:
			dataLength = NOTE_ON_ENTRY_LENGTH;
			break;
		case TYPE_NOTE_OFF_LINK:
			dataLength = NOTE_OFF_ENTRY_LENGTH;
			break;
		case TYPE_CONTROL_CHANGE_LINK:
			dataLength = CONTROL_CHANGE_ENTRY_LENGTH;
			break;
		}
		for (uint8_t i = 0; i < dataLength; i++) { // zapsani do seznamu
			mem2[mem2WritePtr] = usbMessageBuf[i+1];
			mem2WritePtr++;
		}
		reply[0] = OUT_ACK;
		CDC_Transmit_FS(reply, 1);
	}
}
