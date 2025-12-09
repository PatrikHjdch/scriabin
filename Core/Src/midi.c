/*
 * midi.c
 *
 *  Created on: Jun 22, 2025
 *      Author: Patrik
 */

#include "midi.h"
#include <stdio.h>

// deklarace ukazatelu na handlery
void (*midiNoteOnHandler)(uint8_t channel, uint8_t pitch, uint8_t velocity);
void (*midiNoteOffHandler)(uint8_t channel, uint8_t pitch, uint8_t velocity);
void (*midiPolyAftertouchHandler)(uint8_t channel, uint8_t pitch, uint8_t value);
void (*midiControlChangeHandler)(uint8_t channel, uint8_t controller, uint8_t value);
void (*midiProgramChangeHandler)(uint8_t channel, uint8_t value);
void (*midiAftertouchHandler)(uint8_t channel, uint8_t value);
void (*midiPitchBendHandler)(uint8_t channel, uint16_t value);
void (*midiSysExHandler)(uint8_t* message, uint8_t length);
void (*midiMtcQuarterFrameHandler)(uint8_t type, uint8_t value);
void (*midiSongPosHandler)(uint16_t value);
void (*midiSongSelectHandler)(uint8_t value);
void (*midiTuneRequestHandler)();
void (*midiTimingClockHandler)();
void (*midiStartHandler)();
void (*midiContinueHandler)();
void (*midiStopHandler)();
void (*midiActiveSensingHandler)();
void (*midiResetHandler)();

static uint8_t midiInBuffer[MIDI_BUFFER_LENGTH]; // buffer pro prichozi zpravy
static UART_HandleTypeDef* uart; // pointer na instanci uart rozhrani
static DMA_HandleTypeDef* dma; // pointer na instanci dma rozhrani
static uint8_t inputOn; // stav prijimani
static volatile uint16_t midiBufReadPtr; // adresa cteni bufferu
#define midiBufWritePtr (MIDI_BUFFER_LENGTH - dma->Instance->CNDTR) // adresa posledniho zapisu bufferu
#define dataByte1Index (statusByteIndex + 1) % MESSAGE_BUFFER_LENGTH // makra pro zjednoduseni matematickeho zapisu pozice v bufferu pri cteni
#define dataByte2Index (statusByteIndex + 2) % MESSAGE_BUFFER_LENGTH

void midiInit(UART_HandleTypeDef * uartTD, DMA_HandleTypeDef * dmaTD) { // inicializace
	uart = uartTD;
	dma = dmaTD;

	// ve vychozim stavu nezpracovavame zadne zpravy
	midiBufReadPtr = 0;
	midiNoteOnHandler = NULL;
	midiNoteOffHandler = NULL;
	midiPolyAftertouchHandler = NULL;
	midiControlChangeHandler = NULL;
	midiProgramChangeHandler = NULL;
	midiAftertouchHandler = NULL;
	midiPitchBendHandler = NULL;
	midiSysExHandler = NULL;
	midiMtcQuarterFrameHandler = NULL;
	midiSongPosHandler = NULL;
	midiSongSelectHandler = NULL;
	midiTuneRequestHandler = NULL;
	midiStartHandler = NULL;
	midiContinueHandler = NULL;
	midiStopHandler = NULL;
	midiActiveSensingHandler = NULL;
	midiResetHandler = NULL;


	inputOn = 1;
	HAL_UART_Receive_DMA(uart, midiInBuffer, MIDI_BUFFER_LENGTH); // zapnuti prijmu pomoci DMA
}

void midiReceiveOn() {
	inputOn = 1;
}

void midiReceiveOff() {
	inputOn = 0;
}

// funkce pro nastaveni handleru midi zprav
void setNoteOnHandler(void (*func)(uint8_t, uint8_t, uint8_t)) {
	midiNoteOnHandler = func;
}
void setNoteOffHandler(void (*func)(uint8_t, uint8_t, uint8_t)) {
	midiNoteOffHandler = func;
}
void setPolyAftertouchHandler(void (*func)(uint8_t, uint8_t, uint8_t)) {
	midiPolyAftertouchHandler = func;
}
void setControlChangeHandler(void (*func)(uint8_t, uint8_t, uint8_t)) {
	midiControlChangeHandler = func;
}
void setProgramChangeHandler(void (*func)(uint8_t, uint8_t)) {
	midiProgramChangeHandler = func;
}
void setAftertouchHandler(void (*func)(uint8_t, uint8_t)) {
	midiAftertouchHandler = func;
}
void setPitchBendHandler(void (*func)(uint8_t, uint16_t)) {
	midiPitchBendHandler = func;
}
void setSystemExclusiveHandler(void (*func)(uint8_t*, uint8_t)) {
	midiSysExHandler = func;
}
void setQuarterFrameHandler(void (*func)(uint8_t, uint8_t)) {
	midiMtcQuarterFrameHandler = func;
}
void setSongPositionHandler(void (*func)(uint16_t)) {
	midiSongPosHandler = func;
}
void setSongSelectHandler(void (*func)(uint8_t)) {
	midiSongSelectHandler = func;
}
void setTuneRequestHandler(void (*func)()) {
	midiTuneRequestHandler = func;
}
void setStartHandler(void (*func)()) {
	midiStartHandler = func;
}
void setContinueHandler(void (*func)()) {
	midiContinueHandler = func;
}
void setStopHandler(void (*func)()) {
	midiStopHandler = func;
}
void setActiveSensingHandler(void (*func)()) {
	midiActiveSensingHandler = func;
}
void setResetHandler(void (*func)()) {
	midiResetHandler = func;
}

// zpracovavani midi zprav
static void handleMidi(uint8_t data) {
	static uint8_t messageBuf[MESSAGE_BUFFER_LENGTH];
	static uint8_t messageWritePtr = 0;
	uint8_t statusByteIndex;
	if (data >= 0b11111000) { // real-time zpravy se zpracovavaji prednostne
		switch (data) {
		case TIMING_CLOCK:
			if (midiTimingClockHandler) (*midiTimingClockHandler)();
			break;
		case START:
			if (midiStartHandler) (*midiStartHandler)();
			break;
		case CONTINUE:
			if (midiContinueHandler) (*midiContinueHandler)();
			break;
		case STOP:
			if (midiStopHandler) (*midiStopHandler)();
			break;
		case ACTIVE_SENSING:
			if (midiActiveSensingHandler) (*midiActiveSensingHandler)();
			break;
		case RESET:
			if (midiResetHandler) (*midiResetHandler)();
			break;
		}
		return;
	}
	// protoze tohle delame az po kontrole real-time zprav, zachovavame midi zpravu celou
	messageBuf[messageWritePtr] = data;
	messageWritePtr++;
	messageWritePtr = messageWritePtr % MESSAGE_BUFFER_LENGTH;

	if (messageBuf[(messageWritePtr - 2 + MESSAGE_BUFFER_LENGTH) % MESSAGE_BUFFER_LENGTH] > 0x7F) { // zpravy s jednim datovym bytem (kontrola zda minuly byt byl stavovy)
		statusByteIndex = (messageWritePtr - 2 + MESSAGE_BUFFER_LENGTH) % MESSAGE_BUFFER_LENGTH;
		if (messageBuf[statusByteIndex] < SYSTEM_EXCLUSIVE) { // kanalove zpravy
			switch (messageBuf[statusByteIndex] & STATUS_BYTE_TYPE_MASK) {
			case PROGRAM_CHANGE:
				if (midiProgramChangeHandler) (*midiProgramChangeHandler)(messageBuf[statusByteIndex] & STATUS_BYTE_CHANNEL_MASK, messageBuf[dataByte1Index]);
				break;
			case AFTERTOUCH:
				if (midiAftertouchHandler) (*midiAftertouchHandler)(messageBuf[statusByteIndex] & STATUS_BYTE_CHANNEL_MASK, messageBuf[dataByte1Index]);
				break;
			}
		} else { // systemove zpravy
			switch (messageBuf[statusByteIndex]) {
			case MTC_QUARTER_FRAME:
				if (midiMtcQuarterFrameHandler) (*midiMtcQuarterFrameHandler)(messageBuf[dataByte1Index] >> 4, messageBuf[dataByte1Index] & 0b00001111);
				break;
			case SONG_SELECT:
				if (midiSongSelectHandler) (*midiSongSelectHandler)(messageBuf[dataByte1Index]);
				break;
			}
		}
	} else if (messageBuf[(messageWritePtr - 3 + MESSAGE_BUFFER_LENGTH) % MESSAGE_BUFFER_LENGTH] > 0x7F) { // zpravy se dvema datovymy byty (kontrola zda predminuly byt byl stavovy)
		statusByteIndex = (messageWritePtr - 3 + MESSAGE_BUFFER_LENGTH) % MESSAGE_BUFFER_LENGTH;
		if (messageBuf[statusByteIndex] < SYSTEM_EXCLUSIVE) { // kanalove zpravy
			switch (messageBuf[statusByteIndex] & STATUS_BYTE_TYPE_MASK) {
			case NOTE_OFF:
				if (midiNoteOffHandler) (*midiNoteOffHandler)(messageBuf[statusByteIndex] & STATUS_BYTE_CHANNEL_MASK, messageBuf[dataByte1Index], messageBuf[dataByte2Index]);
				break;
			case NOTE_ON:
				if (midiNoteOnHandler) (*midiNoteOnHandler)(messageBuf[statusByteIndex] & STATUS_BYTE_CHANNEL_MASK, messageBuf[dataByte1Index], messageBuf[dataByte2Index]);
				break;
			case POLY_AFTERTOUCH:
				if (midiPolyAftertouchHandler) (*midiPolyAftertouchHandler)(messageBuf[statusByteIndex] & STATUS_BYTE_CHANNEL_MASK, messageBuf[dataByte1Index], messageBuf[dataByte2Index]);
				break;
			case CONTROL_CHANGE:
				if (midiControlChangeHandler) (*midiControlChangeHandler)(messageBuf[statusByteIndex] & STATUS_BYTE_CHANNEL_MASK, messageBuf[dataByte1Index], messageBuf[dataByte2Index]);
				break;
			case PITCH_BEND:
				if (midiPitchBendHandler) (*midiPitchBendHandler)(messageBuf[statusByteIndex] & STATUS_BYTE_CHANNEL_MASK, (messageBuf[dataByte2Index] << 7) + messageBuf[dataByte1Index]);
				break;
			}
		} else { // systemove zpravy
			switch (messageBuf[statusByteIndex]) {
				case SONG_POSITION_POINTER:
					if (midiSongPosHandler) (*midiSongPosHandler)((messageBuf[dataByte2Index] << 7) + messageBuf[dataByte1Index]);
					break;
			}
		}
	} else if (messageBuf[messageWritePtr] == SYSTEM_EXCLUSIVE_END && midiSysExHandler) { // system exclusive
		uint8_t sysExBuf[SYSTEM_EXCLUSIVE_BUFFER_LENGTH];
		uint8_t sysExBufWritePtr = SYSTEM_EXCLUSIVE_BUFFER_LENGTH - 1;
		for (uint8_t i = messageWritePtr + MESSAGE_BUFFER_LENGTH; i > messageWritePtr; i--) {
			if (messageBuf[i % MESSAGE_BUFFER_LENGTH] == SYSTEM_EXCLUSIVE) {
				(*midiSysExHandler)(&sysExBuf[sysExBufWritePtr + 1], SYSTEM_EXCLUSIVE_BUFFER_LENGTH - sysExBufWritePtr - 1 );
				return;
			}
			sysExBuf[sysExBufWritePtr] = messageBuf[i % MESSAGE_BUFFER_LENGTH];
			sysExBufWritePtr--;
		}
	}
}

// cteni midi
void readMidi() {
 	while (midiBufReadPtr != midiBufWritePtr) { // dokud jsou v bufferu nezpracovane byty
		uint8_t receivedByte = midiInBuffer[midiBufReadPtr];
		midiBufReadPtr++;
		midiBufReadPtr = midiBufReadPtr % MIDI_BUFFER_LENGTH;
		if (inputOn) handleMidi(receivedByte);
	}
}

