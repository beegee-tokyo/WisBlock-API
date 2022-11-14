/**
   @file app.h
   @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
   @brief For application specific includes and definitions
   @version 0.1
   @date 2021-04-23

   @copyright Copyright (c) 2021

*/

#ifndef APP_H
#define APP_H

#include <Arduino.h>
/** Add you required includes after Arduino.h */

#include <Wire.h>
/** Include the WisBlock-API */
#include <WisBlock-API.h>

// Debug output set to 0 to disable app debug output
#ifndef MY_DEBUG
#define MY_DEBUG 1
#endif

#ifdef NRF52_SERIES
#if MY_DEBUG > 0
#define MYLOG(tag, ...)                     \
	do                                      \
	{                                       \
		if (tag)                            \
			PRINTF("[%s] ", tag);           \
		PRINTF(__VA_ARGS__);                \
		PRINTF("\n");                       \
		if (g_ble_uart_is_connected)        \
		{                                   \
			g_ble_uart.printf(__VA_ARGS__); \
			g_ble_uart.printf("\n");        \
		}                                   \
	} while (0)
#else
#define MYLOG(...)
#endif
#endif
#ifdef ARDUINO_ARCH_RP2040
#if MY_DEBUG > 0
#define MYLOG(tag, ...)                  \
	do                                   \
	{                                    \
		if (tag)                         \
			Serial.printf("[%s] ", tag); \
		Serial.printf(__VA_ARGS__);      \
		Serial.printf("\n");             \
	} while (0)
#else
#define MYLOG(...)
#endif
#endif
#if defined ESP32
#if MY_DEBUG > 0
#define MYLOG(tag, ...)                                                 \
	if (tag)                                                            \
		Serial.printf("[%s] ", tag);                                    \
	Serial.printf(__VA_ARGS__);                                         \
	Serial.printf("\n");                                                \
	if (g_ble_uart_is_connected)                                        \
	{                                                                   \
		char buff[255];                                                 \
		int len = sprintf(buff, __VA_ARGS__);                           \
		uart_tx_characteristic->setValue((uint8_t *)buff, (size_t)len); \
		uart_tx_characteristic->notify(true);                           \
		delay(50);                                                      \
	}
#else
#define MYLOG(...)
#endif
#endif

/** Application function definitions */
void setup_app(void);
bool init_app(void);
void app_event_handler(void);
void ble_data_handler(void) __attribute__((weak));
void lora_data_handler(void);

// LoRaWan functions
struct lpwan_data_s
{
	uint8_t data_flag1 = 0x08; // 1
	uint8_t data_flag2 = 0x02; // 2
	uint8_t batt_1 = 0;		   // 3
	uint8_t batt_2 = 0;		   // 4
							   // uint8_t data_flag3 = 0x07;	// 5
							   // uint8_t data_flag4 = 0x68;	// 6
							   // uint8_t humid_1 = 0;		// 7
							   // uint8_t data_flag5 = 0x02;	// 8
							   // uint8_t data_flag6 = 0x67;	// 9
							   // uint8_t temp_1 = 0;			// 10
							   // uint8_t temp_2 = 0;			// 11
							   // uint8_t data_flag7 = 0x06;	// 12
							   // uint8_t data_flag8 = 0x73;	// 13
							   // uint8_t press_1 = 0;		// 14
							   // uint8_t press_2 = 0;		// 15
							   // uint8_t data_flag9 = 0x05;	// 16
							   // uint8_t data_flag10 = 0x65; // 17
							   // uint8_t light_1 = 0;		// 18
							   // uint8_t light_2 = 0;		// 19
};
extern lpwan_data_s g_lpwan_data;
#define LPWAN_DATA_LEN 4

/** Battery level uinion */
union batt_s
{
	uint16_t batt16 = 0;
	uint8_t batt8[2];
};

#endif
