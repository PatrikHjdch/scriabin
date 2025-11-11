/*
 * mem.h
 *
 *  Created on: Oct 29, 2025
 *      Author: Patrik
 */

#ifndef INC_MEM_H_
#define INC_MEM_H_

#define CONFIGS 15
#define MEMADRESS 0b1010000
#define WRITE_OPERATION (MEMADRESS << 1) & 0b11111110
#define READ_OPERATION WRITE_OPERATION + 1

#endif /* INC_MEM_H_ */

I2C_HandleTypeDef* i2c;

void init(I2C_HandleTypeDef* i2cInstance);

void writeToMem(uint16_t address, uint8_t value);
uint8_t readFromMem(uint16_t address);
