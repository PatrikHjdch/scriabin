#include "usbComms.h"
#include "stm32f3xx_hal.h"
#include "usbd_cdc_if.h"
#include "asyncUploadManager.h"

static uint8_t usbState = 0;
static uint8_t unhandledFlag = 0;
static uint8_t rxBuf[APP_RX_DATA_SIZE];
static uint32_t rxBufLen;

static void USB_TX_Ping()
{
	static uint8_t buf[] = {OUT_HELLO};
	CDC_Transmit_FS(buf, 1);
}

void USB_TX_ACK()
{
	static uint8_t buf[] = {OUT_ACK};
	CDC_Transmit_FS(buf, 1);
}

void USB_TX_ERR_I2C(uint8_t val)
{
	uint8_t buf[] = { OUT_ERROR, OUT_ERROR_I2C, val };
	CDC_Transmit_FS(buf, 3);
}

void USB_TX_ERR_OUT_OF_ORDER()
{
	static uint8_t buf[] = {OUT_ERROR, OUT_ERROR_LINKS_OUT_OF_ORDER};
	CDC_Transmit_FS(buf, 2);
}

void USB_TX_ERR_INVALID_STATE()
{
	static uint8_t buf[] = {OUT_ERROR, OUT_ERROR_INVALID_STATE};
	CDC_Transmit_FS(buf, 2);
}

void USB_TX_DEBUG_MIDIBYTE(uint8_t byte) {
	uint8_t buf[] = {OUT_DEBUG, OUT_DEBUG_MIDI_BYTE, byte};
	CDC_Transmit_FS(buf, 3);
}

void USB_TX_ERR(uint8_t* err, uint8_t len) {
	uint8_t buf[65];
	buf[0] = OUT_ERROR;
	len = len > 64 ? 64 : len;
	memcpy(&buf[1], err, len);
	CDC_Transmit_FS(buf, len + 1);
}

void setUsbStateOn() {
	usbState = 1;
	HAL_GPIO_WritePin(USB_LED_GPIO_Port, USB_LED_Pin, GPIO_PIN_SET);
}

void setUsbStateOff() {
	usbState = 0;
	HAL_GPIO_WritePin(USB_LED_GPIO_Port, USB_LED_Pin, GPIO_PIN_RESET);
}

void copyIncomingData(uint8_t* pData, uint32_t len) {
	rxBufLen = len;
	memcpy(rxBuf, pData, len);
	unhandledFlag = 1;
}

uint8_t USB_hasUnhandledData() {
	return unhandledFlag;
}

void USB_TX_MIDI_Event(uint8_t* bytes, uint8_t len) {
	if (!usbState) return;
	uint8_t buf[65];
	buf[0] = OUT_EVENT_MIDI_RECEIVED;
	len = len > 64 ? 64 : len;
	memcpy(&buf[1], bytes, len);
	CDC_Transmit_FS(buf, len+1);
}

void USB_TX_DMX_Event(uint16_t c, uint8_t v) {
	if (!usbState) return;
	uint8_t buf[4];
	buf[0] = OUT_EVENT_DMX_CHANGED;
	buf[1] = (c >> 8);
	buf[2] = c & 0xFF;
	buf[3] = v;
	CDC_Transmit_FS(buf, 4);
}

void USB_TX_Profile_Event(uint8_t p) {
	if (!usbState) return;
	uint8_t buf[2];
	buf[0] = OUT_EVENT_PROFILE_CHANGED;
	buf[1] = p;
	CDC_Transmit_FS(buf, 2);
}

void USB_TX_Address_Read_Event(uint8_t* a) {
	if (!usbState) return;
	uint8_t buf[5];
	buf[0] = OUT_EVENT_LINK_ADDRESSES_READ;
	memcpy(&buf[1], a, 4);
	CDC_Transmit_FS(buf, 5);
}

void USB_HandleIncoming() { // zpracovani USB zpravy
	operationResult_t result;
	switch (rxBuf[0]) {
	case IN_HELLO: // testovaci zprava
		USB_TX_Ping();
		break;
	case IN_START_UPLOAD: // zacatek prenosu
		result = uploadBegin();
		break;
	case IN_END_UPLOAD: // konec prenosu
		result = uploadEnd();
		break;
	case IN_PROFILE: // zmena profilu
		result = uploadProfile(rxBuf[1]);
		break;
	case IN_CHANNEL: // zmena kanalu
		result = uploadChannel(rxBuf[1]);
		break;
	case IN_TYPE: // zmena typu vazby
		result = uploadType(rxBuf[1]);
		break;
	case IN_PITCH: // zmena cisla noty
		result = uploadPitch(rxBuf[1]);
		break;
	case IN_NEW_LINK: // nova vazba
		result = uploadLink(&rxBuf[1]);
		break;
	}
	switch (result) {
	case SC_OK:
		break;
	case SC_ERR_INVALID_STATE:
		USB_TX_ERR_INVALID_STATE();
		setState(STATE_BROKEN);
		break;
	case SC_ERR_LINKS_OUT_OF_ORDER:
		USB_TX_ERR_OUT_OF_ORDER();
		setState(STATE_BROKEN);
		break;
	case SC_ERR_I2C:
		USB_TX_ERR_I2C(0);
		setState(STATE_BROKEN);
		break;
	case SC_ERR_UNINITALIZED:
		USB_TX_ERR_INVALID_STATE();
		setState(STATE_BROKEN);
		break;
	}
	unhandledFlag = 0;
}

