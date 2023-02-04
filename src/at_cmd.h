/**
 * @file at_cmd.h
 * @author Taylor Lee (taylor.lee@rakwireless.com)
 * @brief AT command parsing includes & defines
 * @version 0.1
 * @date 2021-04-27
 *
 * @copyright Copyright (c) 2021
 *
 */
#include "WisBlock-API.h"

#ifndef __AT_H__
#define __AT_H__

extern uint8_t g_last_fport;

#ifdef NRF52_SERIES
#define AT_PRINTF(...)                  \
	Serial.printf(__VA_ARGS__);         \
	Serial.printf("\n");                \
	if (g_ble_uart_is_connected)        \
	{                                   \
		g_ble_uart.printf(__VA_ARGS__); \
		g_ble_uart.flush();             \
	}
#endif
#ifdef ESP32
#define AT_PRINTF(...)                                                  \
	Serial.printf(__VA_ARGS__);                                         \
	if (g_ble_uart_is_connected)                                        \
	{                                                                   \
		char buff[255];                                                 \
		int len = sprintf(buff, __VA_ARGS__);                           \
		uart_tx_characteristic->setValue((uint8_t *)buff, (size_t)len); \
		uart_tx_characteristic->notify(true);                           \
		delay(50);                                                      \
	}
#endif
#if defined ARDUINO_ARCH_RP2040
#define AT_PRINTF(...) \
	Serial.printf(__VA_ARGS__);
#endif

#define AT_ERROR "+CME ERROR:"
#define ATCMD_SIZE 160
#define ATQUERY_SIZE 512

#define AT_SUCCESS (0)
#define AT_ERRNO_NOSUPP (1)
#define AT_ERRNO_NOALLOW (2)
#define AT_ERRNO_PARA_VAL (5)
#define AT_ERRNO_PARA_NUM (6)
#define AT_ERRNO_EXEC_FAIL (7)
#define AT_ERRNO_SYS (8)
#define AT_CB_PRINT (0xFF)

// Example of __DATE__ string: "Jul 27 2012"
//                              01234567890

#define BUILD_YEAR_CH0 (__DATE__[7])
#define BUILD_YEAR_CH1 (__DATE__[8])
#define BUILD_YEAR_CH2 (__DATE__[9])
#define BUILD_YEAR_CH3 (__DATE__[10])

#define BUILD_MONTH_IS_JAN (__DATE__[0] == 'J' && __DATE__[1] == 'a' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_FEB (__DATE__[0] == 'F')
#define BUILD_MONTH_IS_MAR (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'r')
#define BUILD_MONTH_IS_APR (__DATE__[0] == 'A' && __DATE__[1] == 'p')
#define BUILD_MONTH_IS_MAY (__DATE__[0] == 'M' && __DATE__[1] == 'a' && __DATE__[2] == 'y')
#define BUILD_MONTH_IS_JUN (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'n')
#define BUILD_MONTH_IS_JUL (__DATE__[0] == 'J' && __DATE__[1] == 'u' && __DATE__[2] == 'l')
#define BUILD_MONTH_IS_AUG (__DATE__[0] == 'A' && __DATE__[1] == 'u')
#define BUILD_MONTH_IS_SEP (__DATE__[0] == 'S')
#define BUILD_MONTH_IS_OCT (__DATE__[0] == 'O')
#define BUILD_MONTH_IS_NOV (__DATE__[0] == 'N')
#define BUILD_MONTH_IS_DEC (__DATE__[0] == 'D')

#define BUILD_MONTH_CH0 \
	((BUILD_MONTH_IS_OCT || BUILD_MONTH_IS_NOV || BUILD_MONTH_IS_DEC) ? '1' : '0')

#define BUILD_MONTH_CH1                                         \
	(                                                           \
		(BUILD_MONTH_IS_JAN) ? '1' : (BUILD_MONTH_IS_FEB) ? '2' \
								 : (BUILD_MONTH_IS_MAR)	  ? '3' \
								 : (BUILD_MONTH_IS_APR)	  ? '4' \
								 : (BUILD_MONTH_IS_MAY)	  ? '5' \
								 : (BUILD_MONTH_IS_JUN)	  ? '6' \
								 : (BUILD_MONTH_IS_JUL)	  ? '7' \
								 : (BUILD_MONTH_IS_AUG)	  ? '8' \
								 : (BUILD_MONTH_IS_SEP)	  ? '9' \
								 : (BUILD_MONTH_IS_OCT)	  ? '0' \
								 : (BUILD_MONTH_IS_NOV)	  ? '1' \
								 : (BUILD_MONTH_IS_DEC)	  ? '2' \
														  : /* error default */ '?')

#define BUILD_DAY_CH0 ((__DATE__[4] >= '0') ? (__DATE__[4]) : '0')
#define BUILD_DAY_CH1 (__DATE__[5])

// Example of __TIME__ string: "21:06:19"
//                              01234567

#define BUILD_HOUR_CH0 (__TIME__[0])
#define BUILD_HOUR_CH1 (__TIME__[1])

#define BUILD_MIN_CH0 (__TIME__[3])
#define BUILD_MIN_CH1 (__TIME__[4])

#define BUILD_SEC_CH0 (__TIME__[6])
#define BUILD_SEC_CH1 (__TIME__[7])

#endif // __AT_H__