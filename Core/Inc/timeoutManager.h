#include <stdint.h>

#define TIMEOUT_SCHEDULE_LENGTH 32 // delka fronty pro timeouty

typedef struct {
	uint32_t timer;
	uint16_t dmxChannel;
	uint8_t dmxValue;
	uint8_t midiChannel;
	uint8_t midiPitch;
} timeoutScheduleEntry;

void timeoutManagerInit();
void scheduleTimeout(uint8_t midiChannel, uint8_t pitch, uint16_t dmxChannel, uint8_t dmxValue, uint16_t timeout);
void disableTimeout(uint8_t midiChannel, uint8_t pitch);
void checkForTimeouts();
