/*
 * usbComms.c
 *
 *  Created on: Nov 7, 2025
 *      Author: Patrik
 */

#include "usbComms.h"
static uint16_t mem1WritePtr;
static uint16_t mem2WritePtr;
static uint8_t currentChannel;
static uint8_t currentType;
static uint8_t currentPitch;

void handleUsb(uint8_t* usbBuf, uint32_t* len) {
	uint8_t command[COMMAND_LENGTH];
	uint8_t reply[COMMAND_LENGTH];
	memcpy(command, usbBuf, COMMAND_LENGTH);
	switch (command) {
	case HELLO:
		reply = OUT_HELLO;
		CDC_Transmit_FS(reply, REPLY_LENGTH);
		break;
	case START_UPLOAD:
		dmxOff();
		mem1WritePtr = 0;
		mem2WritePtr = 0;
		currentChannel = 0;
		currentPitch = 0;
		currentType = 0;
		reply = OUT_ACK;
		CDC_Transmit_FS(reply, REPLY_LENGTH);
		break;
	// TODO: IMPLEMENT MAP UPLOADING
	}
}
