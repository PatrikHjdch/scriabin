/*
 * timeoutManager.c
 *
 *  Created on: Mar 21, 2026
 *      Author: Patrik
 */

#include "timeoutManager.h"
#include "stm32f3xx_hal.h"
#include "dmx.h"

static uint8_t timeoutScheduleWritePtr;
static timeoutScheduleEntry timeoutSchedule[TIMEOUT_SCHEDULE_LENGTH];

void timeoutManagerInit() {
	timeoutScheduleWritePtr = 0;
	for (uint8_t i = 0; i < TIMEOUT_SCHEDULE_LENGTH; i++) {
		timeoutSchedule[i].midiChannel = UINT8_MAX;
		timeoutSchedule[i].midiPitch = UINT8_MAX;
		timeoutSchedule[i].timer = UINT32_MAX;
	}
}

void scheduleTimeout(uint8_t midiChannel, uint8_t pitch, uint16_t dmxChannel, uint8_t dmxValue, uint16_t timeout) {
	timeoutSchedule[timeoutScheduleWritePtr].midiChannel = midiChannel;
	timeoutSchedule[timeoutScheduleWritePtr].midiPitch = pitch;
	timeoutSchedule[timeoutScheduleWritePtr].dmxChannel = dmxChannel;
	timeoutSchedule[timeoutScheduleWritePtr].dmxValue = dmxValue;
	timeoutSchedule[timeoutScheduleWritePtr].timer = HAL_GetTick() + timeout;
	timeoutScheduleWritePtr = (timeoutScheduleWritePtr + 1) % TIMEOUT_SCHEDULE_LENGTH;
}

void disableTimeout(uint8_t midiChannel, uint8_t pitch) {
	for (uint8_t i = 0; i < TIMEOUT_SCHEDULE_LENGTH; i++) {
		if (timeoutSchedule[i].midiChannel == midiChannel && timeoutSchedule[i].midiPitch == pitch) {
			timeoutSchedule[i].midiChannel = UINT8_MAX;
			timeoutSchedule[i].midiPitch = UINT8_MAX;
			timeoutSchedule[i].timer = UINT32_MAX;
		}
	}
}

void checkForTimeouts() {
	for (uint8_t i = 0; i < TIMEOUT_SCHEDULE_LENGTH; i++) {
		if (timeoutSchedule[i].timer <= HAL_GetTick()) {
			dmxWrite(timeoutSchedule[i].dmxChannel, timeoutSchedule[i].dmxValue);
			timeoutSchedule[i].midiChannel = UINT8_MAX;
			timeoutSchedule[i].midiPitch = UINT8_MAX;
			timeoutSchedule[i].timer = UINT32_MAX;
		}
	}
}
