/*
 * midi.c
 *
 *  Created on: Jun 22, 2025
 *      Author: Patrik
 */

#include "midi.h"
#include <stdio.h>

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

static uint8_t midiInBuffer[MIDI_BUFFER_LENGTH];
static volatile uint16_t midiBufReadPtr;
static UART_HandleTypeDef* uart;
static DMA_HandleTypeDef* dma;
static uint8_t inputOn;
#define midiBufWritePtr (MIDI_BUFFER_LENGTH - dma->Instance->CNDTR)

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if (huart == uart) {
		readMidi();
	}
}

void midiInit(UART_HandleTypeDef * uartTD, DMA_HandleTypeDef * dmaInstance) {
	uart = uartTD;
	dma = dmaInstance;
	inputOn = 1;

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

	HAL_UART_Receive_DMA(uart, midiInBuffer, 1);
}

void midiReceiveOn() {
	inputOn = 1;
	HAL_UART_Receive_DMA(uart, midiInBuffer, 1);
}

void midiReceiveOff() {
	inputOn = 0;
}

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

void readMidi() {
	static uint8_t messageBuf[MESSAGE_BUFFER_LENGTH];
	static uint8_t messageWritePtr = 0;
	while(midiBufReadPtr != midiBufWritePtr) {
		messageBuf[messageWritePtr] = midiInBuffer[midiBufReadPtr];
		midiBufReadPtr++;
		midiBufReadPtr = midiBufReadPtr % MIDI_BUFFER_LENGTH;
		if (messageBuf[messageWritePtr] >= 0b11111000) { // real-time messages need to be handled immediately, even between the data bytes of another message
			switch (data) {
			case TIMING_CLOCK:
				(*midiTimingClockHandler)();
				break;
			case START:
				(*midiStartHandler)();
				break;
			case CONTINUE:
				(*midiContinueHandler)();
				break;
			case STOP:
				(*midiStopHandler)();
				break;
			case ACTIVE_SENSING:
				(*midiActiveSensingHandler)();
				break;
			case RESET:
				(*midiResetHandler)();
				break;
			}
			HAL_UART_Receive_DMA(uart, midiInBuffer, 1); // we are missing the one byte that got replaced with the real-time message, so we try receiving it again
			return;
		}
		messageWritePtr++; // we only increment the write pointer if the byte was not a real-time message, by doing this we can overwrite the real-time message with the next data byte, leaving the original midi message intact
	}
	if (messageWritePtr == 1) { // received the status byte in this iteration
		if ( // messages with 2 data bytes
				message[0] == NOTE_ON ||
				message[0] == NOTE_OFF ||
				message[0] == POLY_AFTERTOUCH ||
				message[0] == CONTROL_CHANGE ||
				message[0] == PITCH_BEND ||
				message[0] == SONG_POSITION_POINTER
				) {
			HAL_UART_Receive_DMA(uart, midiInBuffer, 2);
		} else if ( // messages with 1 data byte
				message[0] == PROGRAM_CHANGE ||
				message[0] == AFTERTOUCH ||
				message[0] == MTC_QUARTER_FRAME ||
				message[0] == SONG_SELECT ||
				message[0] == SYSTEM_EXCLUSIVE // unknown length, we need to read it byte by byte
				) {
			HAL_UART_Receive_DMA(uart, midiInBuffer, 1);
		}
	} else if (message[0] < SYSTEM_EXCLUSIVE) { // channel messages (we know the status byte from the previous iteration)
		switch (message[0] & STATUS_BYTE_TYPE_MASK) {
		case NOTE_OFF:
			(*midiNoteOffHandler)(message[0] & STATUS_BYTE_CHANNEL_MASK, message[1], message[2]);
			break;
		case NOTE_ON:
			(*midiNoteOnHandler)(message[0] & STATUS_BYTE_CHANNEL_MASK, message[1], message[2]);
			break;
		case POLY_AFTERTOUCH:
			(*midiPolyAftertouchHandler)(message[0] & STATUS_BYTE_CHANNEL_MASK, message[1], message[2]);
			break;
		case CONTROL_CHANGE:
			(*midiControlChangeHandler)(message[0] & STATUS_BYTE_CHANNEL_MASK, message[1], message[2]);
			break;
		case PROGRAM_CHANGE:
			(*midiProgramChangeHandler)(message[0] & STATUS_BYTE_CHANNEL_MASK, message[1]);
			break;
		case AFTERTOUCH:
			(*midiAftertouchHandler)(message[0] & STATUS_BYTE_CHANNEL_MASK, message[1]);
			break;
		case PITCH_BEND:
			(*midiPitchBendHandler)(message[0] & STATUS_BYTE_CHANNEL_MASK, (message[2] << 7) + message[1]);
			break;
		}
	message[0] = 0;
	messageWritePtr = 0;
	HAL_UART_Receive_DMA(uart, midiInBuffer, 1); // we await another status byte
	} else { // system messages
		switch (message[0]) {
		case SYSTEM_EXCLUSIVE:
			if (message[messageWritePtr - 1] != SYSTEM_EXCLUSIVE_END) {
				HAL_UART_Receive_DMA(uart, midiInBuffer, 1);
				return;
			}
			(*midiSysExHandler)(message[1], messageWritePtr - 2);
			break;
		case MTC_QUARTER_FRAME:
			(*midiMtcQuarterFrameHandler)(message[1] >> 4, message[1] & 0b00001111);
			break;
		case SONG_POSITION_POINTER:
			(*midiSongPosHandler)(message[2] << 7 + message[1]);
			break;
		case SONG_SELECT:
			(*midiSongSelectHandler)(message[1]);
			break;
		}
		message[0] = 0;
		messageWritePtr = 0;
		HAL_UART_Receive_DMA(uart, midiInBuffer, 1); // we await another status byte
	}
}

