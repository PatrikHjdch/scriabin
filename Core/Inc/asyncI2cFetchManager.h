/*
 * asyncI2cFetchManager.h
 *
 *  Created on: Mar 17, 2026
 *      Author: Patrik
 */
#include <stdint.h>
#include "customTypeDef.h"

#define LINK_QUEUE_LENGTH 128

typedef enum myI2C_Status myI2C_Status;

typedef struct linkQueueEntry_t {
	uint8_t profile;
	uint8_t messageType;
	uint8_t channel;
	uint8_t pitch;
	uint8_t velocity;
} linkQueueEntry_t;

typedef enum fetchState_t{
	IDLE,
	RETRIEVING_ADDRESSES,
	RETRIEVING_LINK_DATA
} fetchState_t;

typedef enum memoryReadState_t {
	MEM_READY,
	MEM_ACTIVE,
	MEM_DEFERRED,
	MEM_COMPLETE,
	MEM_ERROR
} memoryReadState_t;

void i2cCheckPendingRequest();
void enqueueMessage(uint8_t profile, uint8_t statusByte, uint8_t channel, uint8_t pitch, uint8_t velocity);
uint8_t i2cIsRequestActive();
void checkForDefferedMemRead();
void handleMidiRequests();
void midiHandler_MemRxCpltCallback(myI2C_Status result);
