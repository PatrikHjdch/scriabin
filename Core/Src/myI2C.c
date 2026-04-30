#include "myI2C.h"
#include "customTypeDef.h"
#include "stm32f302x8.h"
#include "stm32f3xx_hal_tim.h"

typedef enum myI2C_Mode {
	I2C_MODE_WRITE,
	I2C_MODE_READ,
	I2C_MODE_MEM_READ,
	I2C_MODE_MEM_WRITE
} myI2C_Mode;

typedef enum myI2C_MemReadStage {
	I2C_MEM_READ_STAGE_ADDRESSING,
	I2C_MEM_READ_STAGE_READING
} myI2C_MemReadStage;

typedef enum myI2C_Operation {
	I2C_OP_START,
	I2C_OP_DEVADDRESS,
	I2C_OP_MEMADDRESS,
	I2C_OP_DATA_READ,
	I2C_OP_DATA_WRITE,
	I2C_OP_STOP
} myI2C_Operation;

typedef enum myI2C_RWBit {
	I2C_RW_WRITE = 0,
	I2C_RW_READ = 1
} myI2C_RWBit;

static pinStruct_t SDA;
static pinStruct_t SCL;
static TIM_HandleTypeDef* tim;

static volatile myI2C_Status _status = I2C_UNINITIALIZED;
static volatile myI2C_Mode _mode;
static volatile myI2C_MemReadStage _memReadStage;
static volatile myI2C_Operation _currentOperation;
static volatile myI2C_RWBit _rwBit;

static volatile uint8_t _devAddress;
static volatile uint8_t _memAddress[2];
static volatile uint16_t _length;
static volatile uint8_t* _dataBuf;

static volatile uint16_t _dataBufPos;
static volatile uint8_t _bitPos;
static volatile uint8_t _nackFlag;

static void(*_cpltCallback)(myI2C_Status status);

static uint8_t _readSDA() {
	return (SDA.port->IDR & SDA.pin) == 0 ? 0 : 1;
}

static void _setSDA() {
	SDA.port->BSRR = SDA.pin;
}

static void _resetSDA() {
	SDA.port->BRR = SDA.pin;
}

static uint8_t _readSCL() {
	return (SCL.port->IDR & SCL.pin) == 0 ? 0 : 1;
}

static void _setSCL() {
	SCL.port->BSRR = SCL.pin;
}

static void _resetSCL() {
	SCL.port->BRR = SCL.pin;
}

static void _toggleSCL() {
	if (_readSCL()) _resetSCL();
	else _setSCL();
}

static void _disableTimer() {
	tim->Instance->ARR = UINT16_MAX;
	tim->Instance->DIER &= ~TIM_DIER_UIE;
}

static void _enableTimer() {
	tim->Instance->ARR = 5;
	tim->Instance->CNT = 0;
	tim->Instance->DIER |= TIM_DIER_UIE;
}

static void _resetFlagsAndPositions() {
	_dataBufPos = 0;
	_bitPos = 0;
	_nackFlag = 0;
}

static void _finish() {
	_disableTimer();
	_status = I2C_OK;
	if (_cpltCallback) _cpltCallback(_nackFlag ? I2C_ERR : I2C_OK);
}

static void _nextStep() {
	if (_nackFlag) {
		_currentOperation = I2C_OP_STOP; // pokud nedostaneme ACK, predpokladame ze cela komunikace je neplatna, rovnou ukoncime
		return;
	}
	switch (_currentOperation) {
	case I2C_OP_START: // po vygenerovani START vzdy na zacatku musime poslat adresu slave zarizeni
		_currentOperation = I2C_OP_DEVADDRESS;
		_bitPos = 0;
		break;
	case I2C_OP_DEVADDRESS:
		_bitPos = 0;
		_dataBufPos = 0;
		switch (_mode) { // pri jednoduchem cteni a psani primo na sbernici prejdeme hned na cteni/psani
		case I2C_MODE_READ:
			_currentOperation = I2C_OP_DATA_READ;
			break;
		case I2C_MODE_WRITE:
			_currentOperation = I2C_OP_DATA_WRITE;
			break;
		case I2C_MODE_MEM_READ: // pri cteni z pameti zalezi, zda jsme ve fazi adresovani, nebo cteni
			switch (_memReadStage) {
			case I2C_MEM_READ_STAGE_ADDRESSING: // pri cteni z pameti se nejdriv posle instrukce psani na adresu ze ktere chceme cist, ale neposlou se zadna data
				_currentOperation = I2C_OP_MEMADDRESS; // interni pointer v pameti potom ukazuje na tuto adresu
				break;
			case I2C_MEM_READ_STAGE_READING: // ve fazi cteni potom pouze posleme instrukci cteni a pamet zacne posilat data pocinaje temi pod jejim internim pointerem
				_currentOperation = I2C_OP_DATA_READ; // pamet pokracuje ve posilani dat dokud od masteru dostava ACK
			}
			break;
		case I2C_MODE_MEM_WRITE: // pri psani do pameti vzdy posleme instrukci psani na adresu
			_currentOperation = I2C_OP_MEMADDRESS;
			break;
		}
		break;
	case I2C_OP_MEMADDRESS:
		_bitPos = 0;
		_dataBufPos = 0;
		switch (_mode) {
		case I2C_MODE_MEM_READ: // pri cteni z pameti se adresa posle pouze ve fazi adresovani, pote se hned prechazi do faze cteni
			_rwBit = I2C_RW_READ;
			_currentOperation = I2C_OP_START;
			_memReadStage = I2C_MEM_READ_STAGE_READING;
			break;
		case I2C_MODE_MEM_WRITE: // pri psani do pameti se po adrese zacnou zapisovat data
			_currentOperation = I2C_OP_DATA_WRITE;
			break;
		case I2C_MODE_READ: // sem bychom se nemeli dostat
		case I2C_MODE_WRITE:
			break;
		}
		break;
	case I2C_OP_DATA_READ: // po dokonceni cteni nebo zapisu dat se ukonci komunikace
	case I2C_OP_DATA_WRITE:
		_currentOperation = I2C_OP_STOP;
		break;
	case I2C_OP_STOP: // sem bychom se nemeli dostat
		break;
	}
}

static void _stepStart() { // START je definovan jako prepnuti SDA z 1 na 0 zatimco SCL je 1
	static uint8_t nackHandled = 0;
	switch (_readSCL()) {
	case 0: // stane se pouze pokud provadime START REPEAT
		_setSCL(); // SCL 1
		break;
	case 1: // vzdy bude 1 pri prvni iteraci - vstupujeme bud z neaktivniho stavu (SCL = SDA = 1) nebo z ACK bitu (tedy provadime START REPEAT) (SCL = 1, SDA = ?)
		switch (_readSDA()) {
		case 0: // pokud vstupujeme po ack
			_resetSCL(); // SCL 0
			_setSDA(); // SDA 1
			break;
		case 1: // pokud vstupujeme po nack, nebo pokud jsme pripraveni vytvorit START
			if (_nackFlag && !nackHandled) { // pokud vstupujeme po nack (neumim si predstavit situaci, kdy po NACK posilame START REPEAT, ale pro jistotu)
				_resetSCL(); // SCL 0
				nackHandled = 1; // NACK byl osetren
			} else {
				_resetSDA(); // START
				nackHandled = 0; // priprava pro pristi iterace
				_nextStep();
			}
			break;
		}
		break;
	}
}

static void _stepStop() { /// STOP je definovan jako prepnuti SDA z 0 na 1 zatimco SCL je 1
	static uint8_t afterACK = 1;
	switch (_readSCL()) {
	case 0: // pro jistotu by melo vzdy probehnout
		_setSCL();
		break;
	case 1: // vzdy bude 1 pri prvni iteraci - vstupujeme po ACK bitu
		switch (_readSDA()) {
		case 0: // pokud vstupujeme po ACK nebo jsme pripraveni vytvorit STOP
			if (afterACK) {
				_resetSCL();
				_resetSDA();
				afterACK = 0; // ACK byl osetren
			} else {
				_setSDA(); // STOP
				afterACK = 1; // priprava pro pristi iterace
				_finish();
			}
			break;
		case 1: // pokud vstupujeme po NACK
			_resetSCL(); // SCL 0
			_resetSDA(); // SDA 0
			break;
		}
		break;
	}
}

static void _stepDevAddress() {
	switch (_readSCL()) {
	case 0:
		switch (_bitPos) {
		case RW_BIT:
			if (_rwBit) _setSDA();
			else _resetSDA();
			break;
		case ACK_BIT:
			_setSDA();
			break;
		default:
			if ((_devAddress >> (7 - _bitPos)) & 1u) _setSDA();
			else _resetSDA();
			break;
		}
		break;
	case 1:
		if (_bitPos == ACK_BIT) {
			_nackFlag = _readSDA();
			_nextStep();
		}
		else _bitPos++;
		break;
	}
}

static void _stepMemAddress() {
	switch (_readSCL()) {
	case 0:
		if (_bitPos == ACK_BIT) _setSDA();
		else if ((_memAddress[_dataBufPos] >> (7 - _bitPos)) & 1u) _setSDA();
		else _resetSDA();
		break;
	case 1:
		if (_bitPos == ACK_BIT) {
			_nackFlag = _readSDA();
			if (_nackFlag || _dataBufPos) {
				_nextStep();
			} else {
				_bitPos = 0;
				_dataBufPos = 1;
			}
		}
		else _bitPos++;
		break;
	}
}

static void _stepDataWrite() {
	switch (_readSCL()) {
	case 0:
		if (_bitPos == ACK_BIT) _setSDA();
		else if ((_dataBuf[_dataBufPos] >> (7 - _bitPos)) & 1u) _setSDA();
		else _resetSDA();
		break;
	case 1:
		if (_bitPos == ACK_BIT) {
			_nackFlag = _readSDA();
			if (_nackFlag || _dataBufPos == _length - 1) _nextStep();
			else {
				_bitPos = 0;
				_dataBufPos++;
			}
		} else _bitPos++;
		break;
	}
}

static void _stepDataRead() {
	switch (_readSCL()) {
	case 0:
		if (_bitPos == ACK_BIT) {
			_dataBufPos++;
			if (_dataBufPos == _length) {
				_setSDA();
			} else {
				_resetSDA();
			}
		} else _setSDA();
		break;
	case 1:
		if (_bitPos != ACK_BIT) {
			_dataBuf[_dataBufPos]  = (_dataBuf[_dataBufPos] << 1u) + _readSDA();
			_bitPos++;
		}
		else {
			_bitPos = 0;
			if (_dataBufPos == _length)	_nextStep();
		}
		break;
	}
}

static void _stepI2C() {
	switch (_currentOperation) {
	case I2C_OP_START:
	case I2C_OP_STOP:
		break;
	default:
		_toggleSCL();
		break;
	}
	switch (_currentOperation) {
	case I2C_OP_DEVADDRESS:
		_stepDevAddress();
		break;
	case I2C_OP_MEMADDRESS:
		_stepMemAddress();
		break;
	case I2C_OP_DATA_READ:
		_stepDataRead();
		break;
	case I2C_OP_DATA_WRITE:
		_stepDataWrite();
		break;
	case I2C_OP_START:
		_stepStart();
		break;
	case I2C_OP_STOP:
		_stepStop();
		break;
	}
}

void myI2C_TimerElapsedCallback() {
	_stepI2C();
}

void myI2C_RegisterCallback(void (*func)(myI2C_Status)) {
	_cpltCallback = func;
}

void myI2C_Init(pinStruct_t sda, pinStruct_t scl, TIM_HandleTypeDef* htim) {
	SDA = sda;
	SCL = scl;
	tim = htim;
	_setSCL();
	_setSDA();
	_status = I2C_OK;
}

myI2C_Status myI2C_MemRead(uint8_t devAddress, uint16_t memAddress, uint8_t* pData, uint16_t length) {
	if (_status == I2C_OK) {
		_devAddress = devAddress;
		_memAddress[0] = (memAddress >> 8) & 0xFF;
		_memAddress[1] = memAddress & 0xFF;
		_dataBuf = pData;
		_length = length;
		_resetFlagsAndPositions();
		_mode = I2C_MODE_MEM_READ;
		_memReadStage = I2C_MEM_READ_STAGE_ADDRESSING;
		_currentOperation = I2C_OP_START;
		_rwBit = I2C_RW_WRITE;
		for (uint16_t i = 0; i < length; i++) {
			pData[i] = 0;
		}
		_status = I2C_BUSY;
		_enableTimer();
	}
	return I2C_OK;
}

myI2C_Status myI2C_MemWrite(uint8_t devAddress, uint16_t memAddress, uint8_t *pData, uint16_t length) {
	if (_status == I2C_OK) {
		_devAddress = devAddress;
		_memAddress[0] = (memAddress >> 8) & 0xFF;
		_memAddress[1] = memAddress & 0xFF;
		_dataBuf = pData;
		_length = length;
		_resetFlagsAndPositions();
		_mode = I2C_MODE_MEM_WRITE;
		_currentOperation = I2C_OP_START;
		_rwBit = I2C_RW_WRITE;
		_status = I2C_BUSY;
		_enableTimer();
	}
	return I2C_OK;
}

myI2C_Status myI2C_Read(uint8_t devAddress, uint8_t* pData, uint16_t length) {
	if (_status == I2C_OK) {
		_devAddress = devAddress;
		_dataBuf = pData;
		_length = length;
		_resetFlagsAndPositions();
		_mode = I2C_MODE_READ;
		_currentOperation = I2C_OP_START;
		_rwBit = I2C_RW_READ;
		for (uint16_t i = 0; i < length; i++) {
			pData[i] = 0;
		}
		_status = I2C_BUSY;
		_enableTimer();
	}
	return I2C_OK;
}

myI2C_Status myI2C_Write(uint8_t devAddress, uint8_t* pData, uint16_t length) {
	if (_status == I2C_OK) {
		_devAddress = devAddress;
		_dataBuf = pData;
		_length = length;
		_resetFlagsAndPositions();
		_mode = I2C_MODE_WRITE;
		_currentOperation = I2C_OP_START;
		_rwBit = I2C_RW_WRITE;
		_status = I2C_BUSY;
		_enableTimer();
	}
	return I2C_OK;
}
