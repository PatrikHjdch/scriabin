#include <mapProperties.h>
#include "asyncUploadManager.h"
#include "main.h"
#include "myI2C.h"
#include "usbComms.h"

typedef enum mapUploadStage {
	MAP_UPLOAD_START,
	MAP_UPLOAD_PROFILE,
	MAP_UPLOAD_CHANNEL,
	MAP_UPLOAD_TYPE,
	MAP_UPLOAD_PITCH,
	MAP_UPLOAD_LINK,
	MAP_UPLOAD_END,
} mapUploadStage;

static mapUploadStage _stage;
static uint16_t _addressTablePtr;
static uint16_t _addressTablePageLen;
static uint16_t _addressTableEnd;
static uint16_t _linkListPtr;
static uint16_t _linkListPageLen;
static uint16_t _linkListEnd;

static uint8_t _profile;
static uint8_t _channel;
static linkType_t _type;
static uint8_t _pitch;

static uint8_t MEM_VALID = 1;
static uint8_t MEM_INVALID = 0;

static uint8_t _i2cOpFinishedFlag = 0;
static myI2C_Status _i2cOpResult;
static uint8_t _failedAttempts = 0;
static uint8_t _mapValidatedFlag = 0;

static uint8_t _dataBuf[PAGE_BUFFER_LENGTH];
static uint8_t* _linkDataBuf;

void uploader_MemRxCpltCallback(myI2C_Status result) {
	_i2cOpFinishedFlag = 1;
	_i2cOpResult = result;
}

static operationResult_t _invalidateMap() {
	_mapValidatedFlag = 0;
	if (myI2C_MemWrite(ADDRESS_TABLE_ADDRESS, MEM_VALID_ADDRESS, &MEM_INVALID, 1) == I2C_OK) return SC_OK;
	// if (HAL_I2C_Mem_Write_IT(&hi2c1, ADDRESS_TABLE_ADDRESS, MEM_VALID_ADDRESS, I2C_MEMADD_SIZE_16BIT, &MEM_INVALID, 1) == HAL_OK) return SC_OK;
	else return SC_ERR_I2C;
}

static operationResult_t _validateMap() {
	_mapValidatedFlag = 1;
	if (myI2C_MemWrite(ADDRESS_TABLE_ADDRESS, MEM_VALID_ADDRESS, &MEM_VALID, 1) == I2C_OK) return SC_OK;
	// if (HAL_I2C_Mem_Write_IT(&hi2c1, ADDRESS_TABLE_ADDRESS, MEM_VALID_ADDRESS, I2C_MEMADD_SIZE_16BIT, &MEM_VALID, 1) == HAL_OK) return SC_OK;
	else return SC_ERR_I2C;
}

static operationResult_t _addressTablePageWrite() {
	_addressTablePageLen = PAGE_BUFFER_LENGTH - (_addressTablePtr % PAGE_BUFFER_LENGTH);
	if ((_addressTableEnd - _addressTablePtr + 2) < _addressTablePageLen) _addressTablePageLen = _addressTableEnd - _addressTablePtr + 2;
	if (myI2C_MemWrite(ADDRESS_TABLE_ADDRESS, _addressTablePtr, _dataBuf, _addressTablePageLen) == I2C_OK) return SC_OK;
	//if (HAL_I2C_Mem_Write_IT(&hi2c1, ADDRESS_TABLE_ADDRESS, _addressTablePtr, I2C_MEMADD_SIZE_16BIT, _dataBuf, _addressTablePageLen) == HAL_OK) return SC_OK;
	else return SC_ERR_I2C;
}

static operationResult_t _addressTablePageWriteInit() {
	_addressTableEnd = getAddressTableIndex(_profile, _channel, _type, _pitch);
	_addressTablePageLen = 0;
	if (_addressTableEnd < _addressTablePtr) return SC_ERR_LINKS_OUT_OF_ORDER;
	for (uint8_t i = 0; i < PAGE_BUFFER_LENGTH; i+=2) {
		_dataBuf[i] = (_linkListPtr >> 8) & 0xFF;
		_dataBuf[i+1] = _linkListPtr & 0xFF;
	}
	return _addressTablePageWrite();
}

static operationResult_t _linkListPageWrite() {
	_linkListPageLen = PAGE_BUFFER_LENGTH - (_linkListPtr % PAGE_BUFFER_LENGTH);
	if ((_linkListEnd - _linkListPtr + 1) < _linkListPageLen) _linkListPageLen = _linkListEnd - _linkListPtr + 1;
	if (myI2C_MemWrite(LINK_LIST_ADDRESS, _linkListPtr, _linkDataBuf, _linkListPageLen) == I2C_OK) return SC_OK;
	// if (HAL_I2C_Mem_Write_IT(&hi2c1, LINK_LIST_ADDRESS, _linkListPtr, I2C_MEMADD_SIZE_16BIT, &_linkDataBuf[getLinkDataLength(_type) - _linkListPageLen], _linkListPageLen) == HAL_OK) return SC_OK;
	else return SC_ERR_I2C;
}

static operationResult_t _linkListPageWriteInit() {
	_linkListEnd = _linkListPtr + getLinkDataLength(_type) - 1;
	_linkListPageLen = 0;
	return _linkListPageWrite();
}


void mapUploadCheckUpdate() {
	if (!_i2cOpFinishedFlag) return;
	_i2cOpFinishedFlag = 0;
	if (_i2cOpResult == I2C_OK) {
		_failedAttempts = 0;
		switch (_stage) {
		case MAP_UPLOAD_START:
			USB_TX_ACK();
			break;
		case MAP_UPLOAD_PROFILE:
		case MAP_UPLOAD_CHANNEL:
		case MAP_UPLOAD_TYPE:
		case MAP_UPLOAD_PITCH:
			_addressTablePtr += _addressTablePageLen;
			if (_addressTablePtr < _addressTableEnd) {
				_addressTablePageWrite();
			} else {
				USB_TX_ACK();
			}
			break;
		case MAP_UPLOAD_LINK:
			_linkListPtr += _linkListPageLen;
			if (_linkListPtr <= _linkListEnd) {
				_linkDataBuf += _linkListPageLen;
				_linkListPageWrite();
			} else {
				USB_TX_ACK();
			}
			break;
		case MAP_UPLOAD_END:
			_addressTablePtr += _addressTablePageLen;
			if (_addressTablePtr < _addressTableEnd) {
				_addressTablePageWrite();
			} else {
				if (_mapValidatedFlag) {
					USB_TX_ACK();
					setState(STATE_WORKING);
				}
				else _validateMap();
			}
		}
	} else {
		_failedAttempts++;
		if (_failedAttempts < MAX_I2C_ATTEMPTS) {
			HAL_Delay(1u << _failedAttempts);
			switch (_stage) {
			case MAP_UPLOAD_START:
				_invalidateMap();
				break;
			case MAP_UPLOAD_PROFILE:
			case MAP_UPLOAD_CHANNEL:
			case MAP_UPLOAD_TYPE:
			case MAP_UPLOAD_PITCH:
				_addressTablePageWrite();
				break;
			case MAP_UPLOAD_LINK:
				_linkListPageWrite();
				break;
			case MAP_UPLOAD_END:
				if (_addressTablePtr < _addressTableEnd) _addressTablePageWrite();
				else _validateMap();
				break;
			}
		} else {
			USB_TX_ERR_I2C(0);
		}
	}
}

operationResult_t uploadBegin() {
	setState(STATE_UPLOAD);
	_stage = MAP_UPLOAD_START;
	_addressTablePtr = CONFIGS;
	_linkListPtr = 0;
	_profile = 0;
	_channel = 0;
	_type = 0;
	_pitch = 0;
	myI2C_RegisterCallback(uploader_MemRxCpltCallback);
	return _invalidateMap();
}

operationResult_t uploadProfile(uint8_t p) {
	_stage = MAP_UPLOAD_PROFILE;
	_profile = p;
	_channel = 0;
	_type = 0;
	_pitch = 0;
	return _addressTablePageWriteInit();
}

operationResult_t uploadChannel(uint8_t c) {
	_stage = MAP_UPLOAD_CHANNEL;
	_channel = c;
	_type = 0;
	_pitch = 0;
	return _addressTablePageWriteInit();
}

operationResult_t uploadType(linkType_t t) {
	_stage = MAP_UPLOAD_TYPE;
	_type = t;
	_pitch = 0;
	return _addressTablePageWriteInit();
}

operationResult_t uploadPitch(uint8_t p) {
	_stage = MAP_UPLOAD_PITCH;
	_pitch = p;
	return _addressTablePageWriteInit();
}

operationResult_t uploadLink(uint8_t* linkData) {
	_stage = MAP_UPLOAD_LINK;
	_linkDataBuf = linkData;
	return _linkListPageWriteInit();
}

operationResult_t uploadEnd() {
	_stage = MAP_UPLOAD_END;
	_profile = 2;
	_channel = 15;
	_type = 3;
	_pitch = 127;
	return _addressTablePageWriteInit();
}
