/**
 * @file bme680_sensor.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Initialization and reading of BME680 sensor
 * @version 0.1
 * @date 2021-08-21
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "app.h"

Adafruit_BME680 bme;

bool init_bme(void)
{
	if (!bme.begin(0x76, false))
	{
		MYLOG("BME", "Could not find a valid BME680 sensor, check wiring!");
		return false;
	}
	// Set up oversampling and filter initialization
	bme.setTemperatureOversampling(BME680_OS_8X);
	bme.setHumidityOversampling(BME680_OS_2X);
	bme.setPressureOversampling(BME680_OS_4X);
	bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
	bme.setGasHeater(320, 150); // 320*C for 150 ms

	return true;
}

void start_bme(void)
{
	MYLOG("BME", "Start BME reading");
	bme.beginReading();
}

bool read_bme(void)
{
	time_t wait_start = millis();
	bool read_success = false;
	while ((millis() - wait_start) < 5000)
	{
		if (bme.endReading())
		{
			read_success = true;
			break;
		}
	}

	if (!read_success)
	{
		return false;
	}

	int16_t temp_int = (int16_t)(bme.temperature * 10.0);
	uint16_t humid_int = (uint16_t)(bme.humidity * 2);
	uint16_t press_int = (uint16_t)(bme.pressure / 10);
	uint16_t gasres_int = (uint16_t)(bme.gas_resistance / 10);

	g_tracker_data.humid_1 = (uint8_t)(humid_int);
	g_tracker_data.temp_1 = (uint8_t)(temp_int >> 8);
	g_tracker_data.temp_2 = (uint8_t)(temp_int);
	g_tracker_data.press_1 = (uint8_t)(press_int >> 8);
	g_tracker_data.press_2 = (uint8_t)(press_int);
	g_tracker_data.gas_1 = (uint8_t)(gasres_int >> 8);
	g_tracker_data.gas_2 = (uint8_t)(gasres_int);

	MYLOG("BME", "RH= %.2f T= %.2f", (float)(humid_int / 2.0), (float)(temp_int / 10.0));
	MYLOG("BME", "P= %d R= %d", press_int * 10, gasres_int * 10);
	MYLOG("BME", "RH= %.2f T= %.2f", bme.humidity, bme.temperature);
	MYLOG("BME", "P= %ld R= %ld", bme.pressure, bme.gas_resistance);
	return true;
}
