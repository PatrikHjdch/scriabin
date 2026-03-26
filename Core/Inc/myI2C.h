#include <stdint.h>
#include "customTypeDef.h"
#include "stm32f3xx_hal.h"

#define RW_BIT 7
#define ACK_BIT 8

typedef enum myI2C_Status {
	I2C_OK,
	I2C_UNINITIALIZED,
	I2C_BUSY,
	I2C_ERR
} myI2C_Status;

void myI2C_TimerElapsedCallback();
void myI2C_RegisterCallback(void (*func)(myI2C_Status));
void myI2C_Init(pinStruct_t sda, pinStruct_t scl, TIM_HandleTypeDef* htim);

myI2C_Status myI2C_MemRead(uint8_t devAddress, uint16_t memAddress, uint8_t* pData, uint16_t length);
myI2C_Status myI2C_MemWrite(uint8_t devAddress, uint16_t memAddress, uint8_t *pData, uint16_t length);
myI2C_Status myI2C_Read(uint8_t devAddress, uint8_t* pData, uint16_t length);
myI2C_Status myI2C_Write(uint8_t devAddress, uint8_t* pData, uint16_t length);
void myI2C_CheckBusHealth();
