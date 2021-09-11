/**
 * @file app.h
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief 
 * @version 0.1
 * @date 2021-09-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */

/**
 * @file app.h
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief For application specific includes and definitions
 *        Will be included from main.h
 * @version 0.1
 * @date 2021-04-23
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef APP_H
#define APP_H

#include <Arduino.h>
/** Add you required includes after Arduino.h */
#include <Wire.h>

/** Include the SX126x-API */
#include <WisBlock-API.h> // Click to install library: http://librarymanager/All#WisBlock-API

/** Application function definitions */
void setup_app(void);
bool init_app(void);
void app_event_handler(void);
void ble_data_handler(void) __attribute__((weak));
void lora_data_handler(void);

/** Examples for application events */
#define ACC_TRIGGER 0b1000000000000000
#define N_ACC_TRIGGER 0b0111111111111111

/** Application stuff */
extern BaseType_t g_higher_priority_task_woken;

/** Temperature + Humidity stuff */
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h> // Click to install library: http://librarymanager/All#Adafruit_BME680
bool init_bme(void);
bool read_bme(void);
void start_bme(void);

/** Accelerometer stuff */
#include <SparkFunLIS3DH.h> //http://librarymanager/All#SparkFun-LIS3DH
#define INT1_PIN WB_IO3
bool init_acc(void);
void clear_acc_int(void);
void read_acc(void);

// GNSS functions
#include "TinyGPS++.h" //http://librarymanager/All#TinyGPSPlus
bool init_gnss(void);
bool poll_gnss(void);

// LoRaWan functions
struct tracker_data_s
{
	uint8_t data_flag1 = 0x01;	// 1
	uint8_t data_flag2 = 0x88;	// 2
	uint8_t lat_1 = 0;			// 3
	uint8_t lat_2 = 0;			// 4
	uint8_t lat_3 = 0;			// 5
	uint8_t long_1 = 0;			// 6
	uint8_t long_2 = 0;			// 7
	uint8_t long_3 = 0;			// 8
	uint8_t alt_1 = 0;			// 9
	uint8_t alt_2 = 0;			// 10
	uint8_t alt_3 = 0;			// 11
	uint8_t data_flag3 = 0x08;	// 12
	uint8_t data_flag4 = 0x02;	// 13
	uint8_t batt_1 = 0;			// 14
	uint8_t batt_2 = 0;			// 15
	uint8_t data_flag5 = 0x03;	// 16
	uint8_t data_flag6 = 0x71;	// 17
	int8_t acc_x_1 = 0;			// 18
	int8_t acc_x_2 = 0;			// 19
	int8_t acc_y_1 = 0;			// 20
	int8_t acc_y_2 = 0;			// 21
	int8_t acc_z_1 = 0;			// 22
	int8_t acc_z_2 = 0;			// 23
	uint8_t data_flag7 = 0x07;	// 24
	uint8_t data_flag8 = 0x68;	// 25
	uint8_t humid_1 = 0;		// 26
	uint8_t data_flag9 = 0x02;	// 27
	uint8_t data_flag10 = 0x67; // 28
	uint8_t temp_1 = 0;			// 29
	uint8_t temp_2 = 0;			// 30
	uint8_t data_flag11 = 0x06; // 31
	uint8_t data_flag12 = 0x73; // 32
	uint8_t press_1 = 0;		// 33
	uint8_t press_2 = 0;		// 34
	uint8_t data_flag13 = 0x04; // 35
	uint8_t data_flag14 = 0x02; // 36
	uint8_t gas_1 = 0;			// 37
	uint8_t gas_2 = 0;			// 38
};
extern tracker_data_s g_tracker_data;
#define TRACKER_DATA_LEN 38		  // sizeof(g_tracker_data) with BME680
#define TRACKER_DATA_SHORT_LEN 23 // sizeof(g_tracker_data) without BME680

/** Battery level uinion */
union batt_s
{
	uint16_t batt16 = 0;
	uint8_t batt8[2];
};
/** Latitude/Longitude value union */
union latLong_s
{
	uint32_t val32;
	uint8_t val8[4];
};

#endif