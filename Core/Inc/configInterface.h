/*
 * usbComms.h
 *
 *  Created on: Nov 7, 2025
 *      Author: Patrik
 */

#include <string.h>
#include "main.h"
#include "dmx.h"
#include "midi.h"

#ifndef INC_USB_H_
#define INC_USB_H_

// maximalni delka odpovedi
#define REPLY_LENGTH 2

// ciselne hodnoty typu vazeb
#define TYPE_NOTE_LINK 0
#define TYPE_NOTE_ON_LINK 1
#define TYPE_NOTE_OFF_LINK 2
#define TYPE_CONTROL_CHANGE_LINK 3

// pocet adres
#define N_ADDRESSES (uint16_t)(N_CHANNELS * N_TYPES * N_PITCHES * 2 + CONFIGS)

// odchozi zpravy
#define OUT_HELLO 0x01
#define OUT_ACK 0x02
#define OUT_ERROR 0x03
#define OUT_ERROR_LINKS_OUT_OF_ORDER 0x01;

// prichozi zpravy
#define HELLO 0x01
#define ACK 0x02
#define START_UPLOAD 0x03
#define END_UPLOAD 0x04
#define CHANNEL 0x05
#define TYPE 0x06
#define PITCH 0x07
#define NEW_LINK 0x10

// zpristupneni funkci a bufferu
void configInterfaceInit(I2C_HandleTypeDef* i2cHandle);
void handleUsb();
extern uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);
extern uint8_t usbMessageBuf[256];
extern uint32_t usbMessageBufLen;
#endif /* INC_USB_H_ */
