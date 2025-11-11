/*
 * usbComms.h
 *
 *  Created on: Nov 7, 2025
 *      Author: Patrik
 */

#ifndef INC_USB_H_
#define INC_USB_H_

#define COMMAND_LENGTH 3
#define REPLY_LENGTH 3

// Outgoing messages
#define OUT_HELLO "HEL"
#define OUT_ACK "ACK"
#define OUT_ERROR "ERR"

// Incoming messages
#define HELLO "HEL"
#define ACK "ACK"
#define START_UPLOAD "STA"
#define END_UPLOAD "STO"
#define TYPE "TYP"
#define CHANNEL "CHA"
#define PITCH "PIT"

#endif /* INC_USB_H_ */
#include <string.h>
#include "main.h"

void handleUsb(uint8_t* usbBuf, uint32_t* len);
