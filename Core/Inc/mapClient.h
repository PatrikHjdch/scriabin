/*
 * mapClient.h
 *
 *  Created on: Mar 17, 2026
 *      Author: Patrik
 */
#include <stdint.h>

// adresy EEPROM pameti
#define ADDRESS_TABLE_ADDRESS 0b10100010
#define LINK_LIST_ADDRESS 0b10100000
#define INFO_ADDRESS 0b10100110

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
#define CONTROL_CHANGE_LINK_DATA_LENGTH 3

#define PAGE_BUFFER_LENGTH 32

#define LINK_QUEUE_LENGTH 128
#define DUAL_VELOCITY_QUEUE_LENGTH 16

#define MAX_I2C_ATTEMPTS 8

typedef enum myI2C_Status myI2C_Status;

typedef enum linkType_t {
	NOTE_LINK = 0x00,
	NOTE_ON_LINK = 0x01,
	NOTE_OFF_LINK = 0x02,
	CONTROL_CHANGE_LINK = 0x03
} linkType_t;

typedef enum velocityMode_t {
	VELOCITY_MODE_IGNORE = 0,
	VELOCITY_MODE_SINGLE_MESSAGE = 1,
	VELOCITY_MODE_DUAL_MESSAGE_ADD = 2,
	VELOCITY_MODE_DUAL_MESSAGE_MSB_LSB = 3
} velocityMode_t;

typedef struct linkQueueEntry_t {
	uint8_t profile;
	uint8_t messageType;
	uint8_t channel;
	uint8_t pitch;
	uint8_t velocity;
} linkQueueEntry_t;

typedef struct dualVelocityQueueEntry_t {
	velocityMode_t velocityMode;
	uint16_t dmxChannel;
	uint8_t profile;
	uint8_t messageType;
	uint8_t channel;
	uint8_t pitch;
	uint8_t velocity;
} dualVelocityQueueEntry_t;

typedef enum mapClientState_t {
	MAP_IDLE,
	MAP_HANDLE,
	MAP_UPLOAD,
	MAP_DOWNLOAD
} mapClientState_t;

typedef enum mapClientUploadState_t {
	MAP_UPLOAD_START,
	MAP_UPLOAD_PROFILE,
	MAP_UPLOAD_CHANNEL,
	MAP_UPLOAD_TYPE,
	MAP_UPLOAD_PITCH,
	MAP_UPLOAD_LINK,
	MAP_UPLOAD_END,
} mapClientUploadState_t;

typedef enum mapClientHandleState_t {
	MAP_HANDLE_IDLE,
	MAP_HANDLE_ADDRESS,
	MAP_HANDLE_DATA
} mapClientHandleState_t;

typedef enum mapClientReadState_t {
	MAP_READ_READY,
	MAP_READ_ACTIVE,
	MAP_READ_DEFERRED,
	MAP_READ_COMPLETE,
	MAP_READ_ERROR
} mapClientReadState_t;

typedef enum myI2C_Status myI2C_Status;

void map_DualVelocityQueueInit();
uint16_t map_GetAddressTableIndex(uint8_t profile, uint8_t channel, linkType_t type, uint8_t pitch);
uint8_t map_GetLinkDataLength(linkType_t type);
void map_EnqueueMidiHandling(uint8_t profile, uint8_t statusByte, uint8_t channel, uint8_t pitch, uint8_t velocity);
void map_CheckForDefferedMemRead();
void map_StepMidiHandling();
void map_MemRxCpltCallback(myI2C_Status result);

void i2cCheckPendingRequest();
uint8_t i2cIsRequestActive();

void map_StepUpload();
void map_BeginUpload();
void map_SetUploadLinkProfile(uint8_t p);
void map_SetUploadLinkChannel(uint8_t c);
void map_SetUploadLinkType(linkType_t t);
void map_SetUploadLinkPitch(uint8_t p);
void map_WriteUploadLinkData(uint8_t* linkData);
void map_FinishUpload();
void map_Upl_MemRxCpltCallback(myI2C_Status result);
