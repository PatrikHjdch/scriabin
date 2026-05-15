/*
 * asyncI2cFetchManager.c
 *
 *  Created on: Mar 17, 2026
 *      Author: Patrik
 */

#include "stm32f3xx.h"
#include "mapClient.h"
#include "usbAppClient.h"
#include "myI2C.h"
#include "midi.h"
#include "dmx.h"
#include "main.h"

// ----- zpracovavani -----
static mapClientReadState_t memReadState;
static mapClientUploadState_t uploadState;
static mapClientHandleState_t handleState;

// fronta midi zprav
static linkQueueEntry_t midiQueue[LINK_QUEUE_LENGTH];
static uint8_t midiQueueWritePtr = 0;
static uint8_t midiQueueReadPtr = 0;

// fronta dual velocity zprav
static dualVelocityQueueEntry_t dualVelocityQueue[DUAL_VELOCITY_QUEUE_LENGTH];

// informace o soucasne zpracovavane zprave
static linkQueueEntry_t* currentMessage;
static linkType_t linkTypeContext;
static uint8_t currentLinkDataLength;

// adresy pro cteni z eeprom
static uint16_t startAddress;
static uint16_t currentAddress;
static uint16_t endAddress;

static void requestMemRead();

// ----- upload -----
static uint16_t addressTablePtr;
static uint16_t addressTablePageLen;
static uint16_t addressTableEnd;
static uint16_t linkListPtr;
static uint16_t linkListPageLen;
static uint16_t linkListEnd;

static uint8_t profile;
static uint8_t channel;
static linkType_t type;
static uint8_t pitch;

static uint8_t MEM_VALID = 1;
static uint8_t MEM_INVALID = 0;

static uint8_t i2cOpFinishedFlag = 0;
static myI2C_Status i2cOpResult;
static uint8_t failedAttempts = 0;
static uint8_t mapValidatedFlag = 0;

static uint8_t dataBuf[PAGE_BUFFER_LENGTH];
static uint8_t* linkDataBuf;

void map_DualVelocityQueueInit() {
	for (uint8_t i = 0; i < DUAL_VELOCITY_QUEUE_LENGTH; i++) {
		dualVelocityQueue[i].profile = UINT8_MAX;
	}
}

// ----- mapa -----
uint16_t map_GetAddressTableIndex(uint8_t profile, uint8_t channel, linkType_t type, uint8_t pitch) {
	return CONFIGS + 2 * ((uint16_t) profile * N_CHANNELS * N_TYPES * N_PITCHES + (uint16_t)channel * N_TYPES * N_PITCHES + (uint16_t)type * N_PITCHES + (uint16_t)pitch);
}

uint8_t map_GetLinkDataLength(linkType_t type) {
	switch (type) {
	case NOTE_LINK:
		return NOTE_LINK_DATA_LENGTH;
	case NOTE_ON_LINK:
		return NOTE_ON_LINK_DATA_LENGTH;
	case NOTE_OFF_LINK:
		return NOTE_OFF_LINK_DATA_LENGTH;
	case CONTROL_CHANGE_LINK:
		return CONTROL_CHANGE_LINK_DATA_LENGTH;
	}
	return 0;
}

// ----- zpracovavani -----
static uint8_t hasUnprocessedMessages() {
	return midiQueueReadPtr == midiQueueWritePtr ? 0 : 1;
}

void map_MemRxCpltCallback(myI2C_Status result) {
	checkForDeferredDmxTransition();
	if (result == I2C_OK) memReadState = MAP_READ_COMPLETE;
	else if (result == I2C_ERR) {
		USB_TX_ERR_I2C(5);
		requestMemRead();
	} else {
		memReadState = MAP_READ_ERROR;
	}
}

static void retrieveLinkAddresses(uint8_t* dataBuf, uint16_t address) {
	 myI2C_RegisterCallback(map_MemRxCpltCallback);
	 myI2C_MemRead(ADDRESS_TABLE_ADDRESS, address, dataBuf, 4);
}

static void retrieveLinkData(uint8_t* dataBuf, uint16_t address, uint16_t length) {
	 myI2C_RegisterCallback(map_MemRxCpltCallback);
	 myI2C_MemRead(LINK_LIST_ADDRESS, address, dataBuf, length > NOTE_LINK_DATA_LENGTH ? NOTE_LINK_DATA_LENGTH : length);
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
	memReadState = MAP_READ_ACTIVE;
	switch (handleState) {
	case MAP_HANDLE_ADDRESS:
		retrieveLinkAddresses(dataBuf, currentAddress);
		break;
	case MAP_HANDLE_DATA:
		retrieveLinkData(dataBuf, currentAddress, currentLinkDataLength);
		break;
	case MAP_HANDLE_IDLE:
		break;
	}
}

static void requestMemRead() {
	if (dmxGetState() == TX_STATE || dmxGetState() == OFF_STATE) {
		triggerMemRead();
	} else {
		memReadState = MAP_READ_DEFERRED;
	}
}


void map_CheckForDefferedMemRead() {
	if (memReadState == MAP_READ_DEFERRED) {
		triggerMemRead();
	}
}

static void handleDualVelocityLink() {
	uint8_t firstEmpty = UINT8_MAX;
	uint8_t foundMatch = 0;
	uint8_t currentIsMsb = currentMessage->pitch % 2 == 0 ? 1 : 0;
	uint8_t matchPitch = currentIsMsb ? currentMessage->pitch + 1 : currentMessage->pitch - 1;
	for (uint8_t i = 0; i < DUAL_VELOCITY_QUEUE_LENGTH; i++) {
		if (dualVelocityQueue[i].profile == currentMessage->profile &&
				dualVelocityQueue[i].messageType == currentMessage->messageType &&
				dualVelocityQueue[i].channel == currentMessage->channel &&
				dualVelocityQueue[i].pitch == matchPitch &&
				dualVelocityQueue[i].dmxChannel == ((((uint16_t)dataBuf[0]) << 8) | dataBuf[1]) &&
				dualVelocityQueue[i].velocityMode == dataBuf[2]
		) {
			foundMatch = 1;
			dualVelocityQueue[i].profile = UINT8_MAX;
			if (dataBuf[2] == VELOCITY_MODE_DUAL_MESSAGE_ADD) {
				dmxWrite(dualVelocityQueue[i].dmxChannel, dualVelocityQueue[i].velocity + currentMessage->velocity);
			} else {
				dmxWrite(dualVelocityQueue[i].dmxChannel, (currentIsMsb ? ((currentMessage->velocity << 7 ) | dualVelocityQueue[i].velocity) : ((dualVelocityQueue[i].velocity << 7 ) | currentMessage->velocity)) & 0xFF);
			}
			break;
		}
		if (firstEmpty == UINT8_MAX && dualVelocityQueue[i].profile == UINT8_MAX) {
			firstEmpty = i;
		}
	}
	if (!foundMatch && firstEmpty != UINT8_MAX) {
		dualVelocityQueue[firstEmpty].profile = currentMessage->profile;
		dualVelocityQueue[firstEmpty].messageType = currentMessage->messageType;
		dualVelocityQueue[firstEmpty].channel = currentMessage->channel;
		dualVelocityQueue[firstEmpty].pitch = currentMessage->pitch;
		dualVelocityQueue[firstEmpty].dmxChannel = (((uint16_t)dataBuf[0]) << 8) | dataBuf[1];
		dualVelocityQueue[firstEmpty].velocity = currentMessage->velocity;
		dualVelocityQueue[firstEmpty].velocityMode = dataBuf[2];
	}
}

static void handleNoteLink() {
	switch (currentMessage->messageType) {
	case NOTE_ON:
		dmxWrite((((uint16_t)dataBuf[0]) << 8) | dataBuf[1], dataBuf[2] == VELOCITY_MODE_IGNORE ? dataBuf[3] : currentMessage->velocity << 1);
		if ((dataBuf[5] << 8) + dataBuf[6] > 0) { // naplanovani timeoutu
			scheduleTimeout(currentMessage->channel, currentMessage->pitch, // midi kanal a pitch pro identifikaci
					(((uint16_t)dataBuf[0]) << 8) | dataBuf[1], dataBuf[4], // dmx kanal a hodnota
					(((uint16_t)dataBuf[5]) << 8) | dataBuf[6]); // timeout
		}
		break;
	case NOTE_OFF:
		dmxWrite((((uint16_t)dataBuf[0]) << 8) | dataBuf[1], dataBuf[4]);
		if ((dataBuf[5] << 8) + dataBuf[6] > 0) { // zruseni timeoutu
			disableTimeout(currentMessage->channel, currentMessage->pitch);
		}
	}
}

static void handleNoteOnLink() {
	switch (dataBuf[2]) {
	case VELOCITY_MODE_IGNORE:
	case VELOCITY_MODE_SINGLE_MESSAGE:
		dmxWrite((((uint16_t)dataBuf[0]) << 8) | dataBuf[1], dataBuf[2] == VELOCITY_MODE_IGNORE ? dataBuf[3] : currentMessage->velocity << 1);
		break;
	case VELOCITY_MODE_DUAL_MESSAGE_ADD:
	case VELOCITY_MODE_DUAL_MESSAGE_MSB_LSB:
		handleDualVelocityLink();
		break;
	}
}

static void handleNoteOffLink() {
	dmxWrite((((uint16_t)dataBuf[0]) << 8) | dataBuf[1], dataBuf[2]);
}

static void handleControlChangeLink() {
	if (dataBuf[2] == VELOCITY_MODE_SINGLE_MESSAGE) {
		dmxWrite((((uint16_t)dataBuf[0]) << 8) | dataBuf[1], currentMessage->velocity * 2);
	} else {
		handleDualVelocityLink();
	}
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
		currentAddress = map_GetAddressTableIndex(currentMessage->profile, currentMessage->channel, linkTypeContext, currentMessage->pitch);
		handleState = MAP_HANDLE_ADDRESS;
		requestMemRead();
		return 0;
	} else {
		midiQueueReadPtr = (midiQueueReadPtr + 1) % LINK_QUEUE_LENGTH;
		handleState = MAP_HANDLE_IDLE;
		return 1;
	}
}

static uint8_t stepHandling() {
	switch (handleState) {
	case MAP_HANDLE_IDLE:
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
			currentAddress = map_GetAddressTableIndex(currentMessage->profile, currentMessage->channel, linkTypeContext, currentMessage->pitch);
			handleState = MAP_HANDLE_ADDRESS;
			requestMemRead();
		}
		return 0;
	case MAP_HANDLE_ADDRESS:
		if (memReadState != MAP_READ_COMPLETE) return 0;
		startAddress = (((uint16_t)dataBuf[0]) << 8) + dataBuf[1];
		endAddress = (((uint16_t)dataBuf[2]) << 8) + dataBuf[3];
		USB_TX_Address_Read_Event(dataBuf);
		memReadState = MAP_READ_READY;
		if (startAddress == endAddress) return handleEndOfLinks();
		updateDataLength();
		if ((endAddress - startAddress) % currentLinkDataLength != 0 || (endAddress - startAddress) > currentLinkDataLength * 32) { // chyba pri cteni
			requestMemRead(); // precteme znovu
			return 0;
		} else {
			currentAddress = startAddress;
			handleState = MAP_HANDLE_DATA;
			requestMemRead();
			return 0;
		}
	case MAP_HANDLE_DATA:
		if (memReadState == MAP_READ_COMPLETE) {
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
			memReadState = MAP_READ_READY;
			if (currentAddress < endAddress) {
				requestMemRead();
				return 0;
			} else return handleEndOfLinks();
		} else if (memReadState == MAP_READ_ERROR) {

		} else {
			return 0;
		}
	}
	return 0;
}

void map_StepMidiHandling() {
	while (stepHandling());
}

void map_EnqueueMidiHandling(uint8_t profile, uint8_t statusByte, uint8_t channel, uint8_t pitch, uint8_t velocity) {
	midiQueue[midiQueueWritePtr].profile = profile;
	midiQueue[midiQueueWritePtr].messageType = statusByte;
	midiQueue[midiQueueWritePtr].channel = channel;
	midiQueue[midiQueueWritePtr].pitch = pitch;
	midiQueue[midiQueueWritePtr].velocity = velocity;
	midiQueueWritePtr = (midiQueueWritePtr + 1) % LINK_QUEUE_LENGTH;
}

uint8_t i2cIsRequestActive() {
	return memReadState == MAP_READ_ACTIVE;
}

void map_Upl_MemRxCpltCallback(myI2C_Status result) {
	i2cOpFinishedFlag = 1;
	i2cOpResult = result;
}

static void invalidateMap() {
	mapValidatedFlag = 0;
	myI2C_MemWrite(ADDRESS_TABLE_ADDRESS, MEM_VALID_ADDRESS, &MEM_INVALID, 1);
}

static void validateMap() {
	mapValidatedFlag = 1;
	myI2C_MemWrite(ADDRESS_TABLE_ADDRESS, MEM_VALID_ADDRESS, &MEM_VALID, 1);
}

static void addressTablePageWrite() {
	addressTablePageLen = PAGE_BUFFER_LENGTH - (addressTablePtr % PAGE_BUFFER_LENGTH);
	if ((addressTableEnd - addressTablePtr + 2) < addressTablePageLen) addressTablePageLen = addressTableEnd - addressTablePtr + 2;
	myI2C_MemWrite(ADDRESS_TABLE_ADDRESS, addressTablePtr, dataBuf, addressTablePageLen);
}

static void addressTablePageWriteInit() {
	addressTableEnd = map_GetAddressTableIndex(profile, channel, type, pitch);
	addressTablePageLen = 0;
	//if (addressTableEnd < addressTablePtr) return SC_ERR_LINKS_OUT_OF_ORDER;
	for (uint8_t i = 0; i < PAGE_BUFFER_LENGTH; i+=2) {
		dataBuf[i] = (linkListPtr >> 8) & 0xFF;
		dataBuf[i+1] = linkListPtr & 0xFF;
	}
	addressTablePageWrite();
}

static void linkListPageWrite() {
	linkListPageLen = PAGE_BUFFER_LENGTH - (linkListPtr % PAGE_BUFFER_LENGTH);
	if ((linkListEnd - linkListPtr + 1) < linkListPageLen) linkListPageLen = linkListEnd - linkListPtr + 1;
	myI2C_MemWrite(LINK_LIST_ADDRESS, linkListPtr, linkDataBuf, linkListPageLen);
}

static void linkListPageWriteInit() {
	linkListEnd = linkListPtr + map_GetLinkDataLength(type) - 1;
	linkListPageLen = 0;
	linkListPageWrite();
}


void map_StepUpload() {
	if (!i2cOpFinishedFlag) return;
	i2cOpFinishedFlag = 0;
	if (i2cOpResult == I2C_OK) {
		failedAttempts = 0;
		switch (uploadState) {
		case MAP_UPLOAD_START:
			USB_TX_ACK();
			break;
		case MAP_UPLOAD_PROFILE:
		case MAP_UPLOAD_CHANNEL:
		case MAP_UPLOAD_TYPE:
		case MAP_UPLOAD_PITCH:
			addressTablePtr += addressTablePageLen;
			if (addressTablePtr < addressTableEnd) {
				addressTablePageWrite();
			} else {
				USB_TX_ACK();
			}
			break;
		case MAP_UPLOAD_LINK:
			linkListPtr += linkListPageLen;
			if (linkListPtr <= linkListEnd) {
				linkDataBuf += linkListPageLen;
				linkListPageWrite();
			} else {
				USB_TX_ACK();
			}
			break;
		case MAP_UPLOAD_END:
			addressTablePtr += addressTablePageLen;
			if (addressTablePtr < addressTableEnd) {
				addressTablePageWrite();
			} else {
				if (mapValidatedFlag) {
					USB_TX_ACK();
					setState(STATE_WORKING);
				}
				else validateMap();
			}
		}
	} else {
		failedAttempts++;
		if (failedAttempts < MAX_I2C_ATTEMPTS) {
			HAL_Delay(1u << failedAttempts);
			switch (uploadState) {
			case MAP_UPLOAD_START:
				invalidateMap();
				break;
			case MAP_UPLOAD_PROFILE:
			case MAP_UPLOAD_CHANNEL:
			case MAP_UPLOAD_TYPE:
			case MAP_UPLOAD_PITCH:
				addressTablePageWrite();
				break;
			case MAP_UPLOAD_LINK:
				linkListPageWrite();
				break;
			case MAP_UPLOAD_END:
				if (addressTablePtr < addressTableEnd) addressTablePageWrite();
				else validateMap();
				break;
			}
		} else {
			USB_TX_ERR_I2C(0);
		}
	}
}

void map_BeginUpload() {
	setState(STATE_UPLOAD);
	uploadState = MAP_UPLOAD_START;
	addressTablePtr = CONFIGS;
	linkListPtr = 0;
	profile = 0;
	channel = 0;
	type = 0;
	pitch = 0;
	myI2C_RegisterCallback(map_Upl_MemRxCpltCallback);
	invalidateMap();
}

void map_SetUploadLinkProfile(uint8_t p) {
	uploadState = MAP_UPLOAD_PROFILE;
	profile = p;
	channel = 0;
	type = 0;
	pitch = 0;
	addressTablePageWriteInit();
}

void map_SetUploadLinkChannel(uint8_t c) {
	uploadState = MAP_UPLOAD_CHANNEL;
	channel = c;
	type = 0;
	pitch = 0;
	addressTablePageWriteInit();
}

void map_SetUploadLinkType(linkType_t t) {
	uploadState = MAP_UPLOAD_TYPE;
	type = t;
	pitch = 0;
	addressTablePageWriteInit();
}

void map_SetUploadLinkPitch(uint8_t p) {
	uploadState = MAP_UPLOAD_PITCH;
	pitch = p;
	addressTablePageWriteInit();
}

void map_WriteUploadLinkData(uint8_t* linkData) {
	uploadState = MAP_UPLOAD_LINK;
	linkDataBuf = linkData;
	linkListPageWriteInit();
}

void map_FinishUpload() {
	uploadState = MAP_UPLOAD_END;
	profile = 2;
	channel = 15;
	type = 3;
	pitch = 127;
	addressTablePageWriteInit();
}
