/*
 * midi.h
 *
 *  Created on: Jun 22, 2025
 *      Author: Patrik
 */
#include "main.h"
#include <stdint.h>

#ifndef INC_MIDI_H_
#define INC_MIDI_H_

#define USING_IT_MIDI
//#define USING_DMA_MIDI

#define MIDI_BUFFER_LENGTH 64
#define MESSAGE_BUFFER_LENGTH 64
#define SYSTEM_EXCLUSIVE_BUFFER_LENGTH 64

// KANALOVE ZPRAVY -----------------
#define NOTE_ON 		0b10010000
#define NOTE_OFF 		0b10000000
#define POLY_AFTERTOUCH 0b10100000
#define CONTROL_CHANGE	0b10110000
#define PROGRAM_CHANGE	0b11000000
#define	AFTERTOUCH		0b11010000
#define PITCH_BEND		0b11100000

// SYSTEMOVE ZPRAVY ------------------
#define SYSTEM_EXCLUSIVE		0b11110000
#define SYSTEM_EXCLUSIVE_END	0b11110111
#define MTC_QUARTER_FRAME		0b11110001
#define SONG_POSITION_POINTER	0b11110010
#define SONG_SELECT				0b11110011
#define TUNE_REQUEST			0b11110110

// REALTIME ZPRAVY ---------
#define TIMING_CLOCK	0b11111000
#define START			0b11111010
#define CONTINUE		0b11111011
#define STOP			0b11111100
#define ACTIVE_SENSING	0b11111110
#define RESET			0b11111111

// masky pro rozdeleni detekce typu a kanalu
#define STATUS_BYTE_TYPE_MASK 0b11110000
#define STATUS_BYTE_CHANNEL_MASK 0b00001111
#endif /* INC_MIDI_H_ */

// zpristupneni ukazatelu na handlery
extern void (*midiNoteOnHandler)(uint8_t channel, uint8_t pitch, uint8_t velocity);
extern void (*midiNoteOffHandler)(uint8_t channel, uint8_t pitch, uint8_t velocity);
extern void (*midiPolyAftertouchHandler)(uint8_t channel, uint8_t pitch, uint8_t value);
extern void (*midiControlChangeHandler)(uint8_t channel, uint8_t controller, uint8_t value);
extern void (*midiProgramChangeHandler)(uint8_t channel, uint8_t value);
extern void (*midiAftertouchHandler)(uint8_t channel, uint8_t value);
extern void (*midiPitchBendHandler)(uint8_t channel, uint16_t value);
extern void (*midiSysExHandler)(uint8_t* message, uint8_t length);
extern void (*midiMtcQuarterFrameHandler)(uint8_t type, uint8_t value);
extern void (*midiSongPosHandler)(uint16_t value);
extern void (*midiSongSelectHandler)(uint8_t value);
extern void (*midiTuneRequestHandler)();
extern void (*midiTimingClockHandler)();
extern void (*midiStartHandler)();
extern void (*midiContinueHandler)();
extern void (*midiStopHandler)();
extern void (*midiActiveSensingHandler)();
extern void (*midiResetHandler)();


// zpristupneni funkci
#ifdef USING_IT_MIDI
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void midiInit(UART_HandleTypeDef * uart);
#elif defined USING_DMA_MIDI
void midiInit(UART_HandleTypeDef * uart, DMA_HandleTypeDef * dmaC);
#endif
void readMidi();
void midiReceiveOn();
void midiReceiveOff();

void setNoteOnHandler(void (*func)(uint8_t, uint8_t, uint8_t));
void setNoteOffHandler(void (*func)(uint8_t, uint8_t, uint8_t));
void setPolyAftertouchHandler(void (*func)(uint8_t, uint8_t, uint8_t));
void setControlChangeHandler(void (*func)(uint8_t, uint8_t, uint8_t));
void setProgramChangeHandler(void (*func)(uint8_t, uint8_t));
void setAftertouchHandler(void (*func)(uint8_t, uint8_t));
void setPitchBendHandler(void (*func)(uint8_t, uint16_t));
void setSystemExclusiveHandler(void (*func)(uint8_t*, uint8_t));
void setQuarterFrameHandler(void (*func)(uint8_t, uint8_t));
void setSongPositionHandler(void (*func)(uint16_t));
void setSongSelectHandler(void (*func)(uint8_t));
void setTuneRequestHandler(void (*func)());
void setStartHandler(void (*func)());
void setContinueHandler(void (*func)());
void setStopHandler(void (*func)());
void setActiveSensingHandler(void (*func)());
void setResetHandler(void (*func)());
