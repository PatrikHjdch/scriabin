/*
 * asyncI2cFetchManager.c
 *
 *  Created on: Mar 17, 2026
 *      Author: Patrik
 */

#include "asyncI2cFetchManager.h"
#include "stm32f3xx.h"
#include "myI2C.h"
#include "midi.h"
#include "dmx.h"
#include "main.h"
#include "usbComms.h"
#include "mapProperties.h"

// stav
static fetchState_t state = IDLE;
static memoryReadState_t memReadState = MEM_READY;

// fronta midi zprav
static linkQueueEntry_t midiQueue[LINK_QUEUE_LENGTH];
static uint8_t midiQueueWritePtr = 0;
static uint8_t midiQueueReadPtr = 0;

// informace o soucasne zpracovavane zprave
static linkQueueEntry_t* currentMessage;
static linkType_t linkTypeContext;
static uint8_t currentLinkDataLength;

// adresy pro cteni z eeprom
static uint16_t startAddress;
static uint16_t currentAddress;
static uint16_t endAddress;

// buffer pro cteni z eeprom
static uint8_t linkDataBuf[NOTE_LINK_DATA_LENGTH];
static void requestMemRead();

static uint8_t hasUnprocessedMessages() {
	return midiQueueReadPtr == midiQueueWritePtr ? 0 : 1;
}

void midiHandler_MemRxCpltCallback(myI2C_Status result) {
	checkForDeferredDmxTransition();
	if (result == I2C_OK) memReadState = MEM_COMPLETE;
	else if (result == I2C_ERR) {
		USB_TX_ERR_I2C(5);
		requestMemRead();
	} else {
		memReadState = MEM_ERROR;
	}
}

static void _retrieveLinkAddresses(uint8_t* dataBuf, uint16_t address) {
	 myI2C_RegisterCallback(midiHandler_MemRxCpltCallback);
	 myI2C_MemRead(ADDRESS_TABLE_ADDRESS, address, dataBuf, 4);
	 //HAL_I2C_Mem_Read_IT(&hi2c1, ADDRESS_TABLE_ADDRESS, address, I2C_MEMADD_SIZE_16BIT, dataBuf, 4);
}

static void _retrieveLinkData(uint8_t* dataBuf, uint16_t address, uint16_t length) {
	 myI2C_RegisterCallback(midiHandler_MemRxCpltCallback);
	 myI2C_MemRead(LINK_LIST_ADDRESS, address, dataBuf, length > NOTE_LINK_DATA_LENGTH ? NOTE_LINK_DATA_LENGTH : length);
	// HAL_I2C_Mem_Read_IT(&hi2c1, LINK_LIST_ADDRESS, address, I2C_MEMADD_SIZE_16BIT, dataBuf, length > NOTE_LINK_DATA_LENGTH ? NOTE_LINK_DATA_LENGTH : length);
}

static void updateDataLength() {
	switch (linkTypeContext) {
	case NOTE_LINK:
		currentLinkDataLength = NOTE_LINK_DATA_LENGTH;
		break;
	case NOTE_ON_LINK:
		currentLinkDataLength = NOTE_ON_LINK_DATA_LENGTH;
		break;
	case NOTE_OFF_LINK:
		currentLinkDataLength = NOTE_OFF_LINK_DATA_LENGTH;
		break;
	case CONTROL_CHANGE_LINK:
		currentLinkDataLength = CONTROL_CHANGE_LINK_DATA_LENGTH;
		break;
	}
}

static void triggerMemRead() {
	memReadState = MEM_ACTIVE;
	switch (state) {
	case RETRIEVING_ADDRESSES:
		_retrieveLinkAddresses(linkDataBuf, currentAddress);
		break;
	case RETRIEVING_LINK_DATA:
		_retrieveLinkData(linkDataBuf, currentAddress, currentLinkDataLength);
		break;
	case IDLE: // melo by byt nemozne
		break;
	}
}

static void requestMemRead() {
	if (dmxGetState() == TX_STATE || dmxGetState() == OFF_STATE) {
		triggerMemRead();
	} else {
		memReadState = MEM_DEFERRED;
	}
}


void checkForDefferedMemRead() {
	if (memReadState == MEM_DEFERRED) {
		triggerMemRead();
	}
}

static void handleNoteLink() {
	switch (currentMessage->messageType) {
	case NOTE_ON:
		dmxWrite((((uint16_t)linkDataBuf[0]) << 8) + linkDataBuf[1], linkDataBuf[2] == 1 ? currentMessage->velocity * 2 : linkDataBuf[3]);
		if ((linkDataBuf[5] << 8) + linkDataBuf[6] > 0) { // naplanovani timeoutu
			scheduleTimeout(currentMessage->channel, currentMessage->pitch, // midi kanal a pitch pro identifikaci
					(((uint16_t)linkDataBuf[0]) << 8) + linkDataBuf[1], linkDataBuf[4], // dmx kanal a hodnota
					(((uint16_t)linkDataBuf[5]) << 8) + linkDataBuf[6]); // timeout
		}
		break;
	case NOTE_OFF:
		dmxWrite((((uint16_t)linkDataBuf[0]) << 8) + linkDataBuf[1], linkDataBuf[4]);
		if ((linkDataBuf[5] << 8) + linkDataBuf[6] > 0) { // ruseni timeoutu
			disableTimeout(currentMessage->channel, currentMessage->pitch);
		}
	}
}

static void handleNoteOnLink() {
	dmxWrite((((uint16_t)linkDataBuf[0]) << 8) + linkDataBuf[1], linkDataBuf[2] == 1 ? currentMessage->velocity * 2 : linkDataBuf[3]);
}

static void handleNoteOffLink() {
	dmxWrite((((uint16_t)linkDataBuf[0]) << 8) + linkDataBuf[1], linkDataBuf[2]);
}

static void handleControlChangeLink() {
	dmxWrite((((uint16_t)linkDataBuf[0]) << 8) + linkDataBuf[1], currentMessage->velocity * 2);
}

static uint8_t handleEndOfLinks() {
	if (linkTypeContext == NOTE_LINK) {
		switch (currentMessage->messageType) {
		case NOTE_ON:
			linkTypeContext = NOTE_ON_LINK;
			break;
		case NOTE_OFF:
			linkTypeContext = NOTE_OFF_LINK;
			break;
		}
		currentAddress = getAddressTableIndex(currentMessage->profile, currentMessage->channel, linkTypeContext, currentMessage->pitch);
		state = RETRIEVING_ADDRESSES;
		requestMemRead();
		return 0;
	} else {
		midiQueueReadPtr = (midiQueueReadPtr + 1) % LINK_QUEUE_LENGTH;
		state = IDLE;
		return 1;
	}
}

static uint8_t nextAction() {
	switch (state) {
	case IDLE:
		if (hasUnprocessedMessages()) {
			currentMessage = &midiQueue[midiQueueReadPtr];
			switch (currentMessage->messageType) {
			case NOTE_ON:
			case NOTE_OFF:
				linkTypeContext = NOTE_LINK;
				break;
			case CONTROL_CHANGE:
				linkTypeContext = CONTROL_CHANGE_LINK;
				break;
			}
			currentAddress = getAddressTableIndex(currentMessage->profile, currentMessage->channel, linkTypeContext, currentMessage->pitch);
			state = RETRIEVING_ADDRESSES;
			requestMemRead();
		}
		return 0;
	case RETRIEVING_ADDRESSES:
		if (memReadState != MEM_COMPLETE) return 0;
		startAddress = (((uint16_t)linkDataBuf[0]) << 8) + linkDataBuf[1];
		endAddress = (((uint16_t)linkDataBuf[2]) << 8) + linkDataBuf[3];
		USB_TX_Address_Read_Event(linkDataBuf);
		memReadState = MEM_READY;
		if (startAddress == endAddress) return handleEndOfLinks();
		updateDataLength();
		if ((endAddress - startAddress) % currentLinkDataLength != 0 || (endAddress - startAddress) > currentLinkDataLength * 32) { // chyba pri cteni
			requestMemRead(); // precteme znovu
			return 0;
		} else {
			currentAddress = startAddress;
			state = RETRIEVING_LINK_DATA;
			requestMemRead();
			return 0;
		}
	case RETRIEVING_LINK_DATA:
		if (memReadState == MEM_COMPLETE) {
			currentAddress += currentLinkDataLength;
			switch (linkTypeContext) {
			case NOTE_LINK:
				handleNoteLink();
				break;
			case NOTE_ON_LINK:
				handleNoteOnLink();
				break;
			case NOTE_OFF_LINK:
				handleNoteOffLink();
				break;
			case CONTROL_CHANGE_LINK:
				handleControlChangeLink();
				break;
			}
			memReadState = MEM_READY;
			if (currentAddress < endAddress) {
				requestMemRead();
				return 0;
			} else return handleEndOfLinks();
		} else if (memReadState == MEM_ERROR) {

		} else {
			return 0;
		}
	}
	return 0;
}

void handleMidiRequests() {
	while (nextAction());
}

void enqueueMessage(uint8_t profile, uint8_t statusByte, uint8_t channel, uint8_t pitch, uint8_t velocity) {
	midiQueue[midiQueueWritePtr].profile = profile;
	midiQueue[midiQueueWritePtr].messageType = statusByte;
	midiQueue[midiQueueWritePtr].channel = channel;
	midiQueue[midiQueueWritePtr].pitch = pitch;
	midiQueue[midiQueueWritePtr].velocity = velocity;
	midiQueueWritePtr = (midiQueueWritePtr + 1) % LINK_QUEUE_LENGTH;
}

uint8_t i2cIsRequestActive() {
	return memReadState == MEM_ACTIVE;
}
