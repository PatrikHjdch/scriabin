/*
 * asyncUploadManager.h
 *
 *  Created on: Mar 23, 2026
 *      Author: Patrik
 */
#include <stdint.h>
#include "customTypeDef.h"

typedef enum myI2C_Status myI2C_Status;

#define MAX_I2C_ATTEMPTS 8

void mapUploadCheckUpdate();
operationResult_t uploadBegin();
operationResult_t uploadProfile(uint8_t p);
operationResult_t uploadChannel(uint8_t c);
operationResult_t uploadType(linkType_t t);
operationResult_t uploadPitch(uint8_t p);
operationResult_t uploadLink(uint8_t* linkData);
operationResult_t uploadEnd();
void uploader_MemRxCpltCallback(myI2C_Status result);
