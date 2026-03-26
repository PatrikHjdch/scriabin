/*
 * customTypeDef.h
 *
 *  Created on: Mar 16, 2026
 *      Author: Patrik
 */
#ifndef USING_CUSTOMTYPEDEF
#define USING_CUSTOMTYPEDEF
#include "stm32f302x8.h"

typedef enum mainState_t {
	STATE_BROKEN,
	STATE_WORKING,
	STATE_UPLOAD
} mainState_t;

typedef enum linkType_t {
	NOTE_LINK = 0x00,
	NOTE_ON_LINK = 0x01,
	NOTE_OFF_LINK = 0x02,
	CONTROL_CHANGE_LINK = 0x03
} linkType_t;

// pouzivame prefix SC pro zamezeni mozneho zameneni s jinymi konstantami
typedef enum operationResult_t {
	SC_OK = 0x00,
	SC_ERR_INVALID_STATE = 0x01,
	SC_ERR_I2C = 0x02,
	SC_ERR_LINKS_OUT_OF_ORDER = 0x03,
	SC_ERR_UNINITALIZED = 0x04,
} operationResult_t;

typedef struct pinStruct_t{
	GPIO_TypeDef* port;
	uint16_t pin;
} pinStruct_t;

#endif
