#include <stdint.h>
#include "customTypeDef.h"

// odchozi zpravy
#define OUT_HELLO 0x01
#define OUT_ACK 0x02
#define OUT_ERROR 0x03
#define OUT_DEBUG 0x04

#define OUT_ERROR_LINKS_OUT_OF_ORDER 0x01
#define OUT_ERROR_I2C 0x02
#define OUT_ERROR_INVALID_STATE 0x03

#define OUT_DEBUG_MIDI_BYTE 0x01

#define OUT_EVENT_MIDI_RECEIVED 0x10
#define OUT_EVENT_DMX_CHANGED 0x11
#define OUT_EVENT_PROFILE_CHANGED 0x12
#define OUT_EVENT_LINK_ADDRESSES_READ 0x13

// prichozi zpravy
#define IN_HELLO 0x01
#define IN_ACK 0x02
#define IN_START_UPLOAD 0x03
#define IN_END_UPLOAD 0x04
#define IN_CHANNEL 0x05
#define IN_TYPE 0x06
#define IN_PITCH 0x07
#define IN_PROFILE 0x08
#define IN_NEW_LINK 0x10

void setUsbStateOn();
void setUsbStateOff();
void copyIncomingData(uint8_t* pData, uint32_t len);
uint8_t USB_hasUnhandledData();
void USB_HandleIncoming();

void USB_TX_ACK();
void USB_TX_ERR_I2C(uint8_t val);
void USB_TX_ERR_OUT_OF_ORDER();
void USB_TX_ERR_INVALID_STATE();
void USB_TX_ERR(uint8_t* err, uint8_t len);

void USB_TX_DEBUG_MIDIBYTE(uint8_t byte);

void USB_TX_MIDI_Event(uint8_t* bytes, uint8_t len);
void USB_TX_DMX_Event(uint16_t c, uint8_t v);
void USB_TX_Profile_Event(uint8_t p);
void USB_TX_Address_Read_Event(uint8_t* a);
