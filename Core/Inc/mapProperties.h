#include <stdint.h>
#include "customTypeDef.h"

#define ADDRESS_TABLE_ADDRESS 0b10100010
#define LINK_LIST_ADDRESS 0b10100000

#define CONFIGS 16 // delka useku pameti rezervovaneho pro dalsi nastaveni
#define MEM_VALID_ADDRESS 0 // zatim nepouzite - bude se pouzivat k verifikaci uspesneho zapsani dat (na zacatku prepisu se prepne na 0, na konci zase na 1)

#define N_PROFILES 3 // pocet profilu
#define N_CHANNELS 16 // pocet kanalu
#define N_PITCHES 128 // pocet cisel not
#define N_TYPES 4 // pocet typu
#define N_ADDRESSES (uint16_t)(N_PROFILES * N_CHANNELS * N_TYPES * N_PITCHES * 2 + CONFIGS)

#define NOTE_LINK_DATA_LENGTH 7
#define NOTE_ON_LINK_DATA_LENGTH 4
#define NOTE_OFF_LINK_DATA_LENGTH 3
#define CONTROL_CHANGE_LINK_DATA_LENGTH 2

#define PAGE_BUFFER_LENGTH 32

uint16_t getAddressTableIndex(uint8_t profile, uint8_t channel, linkType_t type, uint8_t pitch);
uint8_t getLinkDataLength(linkType_t type);
