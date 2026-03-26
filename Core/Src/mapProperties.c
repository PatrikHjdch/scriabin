#include "mapProperties.h"

uint16_t getAddressTableIndex(uint8_t profile, uint8_t channel, linkType_t type, uint8_t pitch) {
	return CONFIGS + 2 * ((uint16_t) profile * N_CHANNELS * N_TYPES * N_PITCHES + (uint16_t)channel * N_TYPES * N_PITCHES + (uint16_t)type * N_PITCHES + (uint16_t)pitch);
}

uint8_t getLinkDataLength(linkType_t type) {
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
