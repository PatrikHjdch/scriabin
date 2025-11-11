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
static DMA_HandleTypeDef* dma;
#define midiBufWritePtr (MIDI_BUFFER_LENGTH - dma->Instance->CNDTR)

void midiInit(UART_HandleTypeDef * uart, DMA_HandleTypeDef * dmaInstance) {
	dma = dmaInstance;

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

	HAL_UART_Receive_DMA(uart, midiInBuffer, MIDI_BUFFER_LENGTH);
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


static void midiByteAvailable(uint8_t data) {
	static uint16_t position;
	static uint8_t message[MESSAGE_BUFFER_LENGTH];
	if (data >= 0b11111000) {
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
		return;
	}
	if (position == 0 && data < 0b10000000) return;
	if (position < MESSAGE_BUFFER_LENGTH) message[position++] = data;
	position = position % MESSAGE_BUFFER_LENGTH;
	if (message[0] < 0b11110000) {
		switch (message[0] & 0b11110000) {
		case NOTE_ON:
			if (position == 3) {
				(*midiNoteOnHandler)(message[0] & 0b00001111, message[1], message[2]);
				position = 0;
			}
			break;
		case NOTE_OFF:
			if (position == 3) {
				(*midiNoteOffHandler)(message[0] & 0b00001111, message[1], message[2]);
				position = 0;
			}
			break;
		case POLY_AFTERTOUCH:
			if (position == 3) {
				(*midiPolyAftertouchHandler)(message[0] & 0b00001111, message[1], message[2]);
				position = 0;
			}
			break;
		case CONTROL_CHANGE:
			if (position == 3) {
				(*midiControlChangeHandler)(message[0] & 0b00001111, message[1], message[2]);
				position = 0;
			}
			break;
		case PROGRAM_CHANGE:
			if (position == 2) {
				(*midiProgramChangeHandler)(message[0] & 0b00001111, message[1]);
				position = 0;
			}
			break;
		case AFTERTOUCH:
			if (position == 3) {
				(*midiAftertouchHandler)(message[0] & 0b00001111, message[1]);
				position = 0;
			}
			break;
		case PITCH_BEND:
			if (position == 3) {
				(*midiPitchBendHandler)(message[0] & 0b00001111, (message[2] << 7) | message[1]);
				position = 0;
			}
			break;
		}
	} else {
		switch(message[0]) {
		case SYSTEM_EXCLUSIVE:
			if (message[position-1] == SYSTEM_EXCLUSIVE_END) {
				(*midiSysExHandler)(&message[1], position-2);
				position = 0;
			}
			break;
		case MTC_QUARTER_FRAME:
			if (position == 2) {
				(*midiMtcQuarterFrameHandler)((message[1] >> 4) & 0b00000111, message[1] & 0b00001111);
				position = 0;
			}
			break;
		case SONG_POSITION_POINTER:
			if (position == 3) {
				(*midiSongPosHandler)((message[2] << 7) | message[1]);
				position = 0;
			}
			break;
		case SONG_SELECT:
			if (position == 2) {
				(*midiSongSelectHandler)(message[1]);
				position = 0;
			}
			break;
		case TUNE_REQUEST:
			(*midiTuneRequestHandler)();
			position = 0;
			break;
		}
	}
}

void readMidi() {
	while (midiBufReadPtr != midiBufWritePtr) {
		uint8_t data = midiInBuffer[midiBufReadPtr];
		midiBufReadPtr++;
		midiBufReadPtr = midiBufReadPtr % MIDI_BUFFER_LENGTH;
		midiByteAvailable(data);
	}
}
