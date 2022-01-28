/**
 * @file bme680_sensor.ino
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Initialization and data aquisition for Bosch BME680
 * @version 0.1
 * @date 2021-09-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "main.h"
#include <Arduino.h>

extern uint8_t collected_data[];

/** BME680 */
Adafruit_BME680 bme;

// Might need adjustments
#define SEALEVELPRESSURE_HPA (1010.0)

/**
 * @brief Initialize BME680 sensor
 * 
 * @return true if initialization success
 * @return false if initialization failed
 */
bool init_bme680(void)
{
	Wire.begin();

	// Start connection to BME680
	if (!bme.begin(0x76))
	{
		MYLOG("APP", "Could not find a valid BME680 sensor, check wiring!");
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

/**
 * @brief Read sensor data from the BME680
 *        Writes received sensor data directly into the LoRaWAN packet
 *        Data decoding is compatible with RAK7205
 *        Chirpstack & TTN encoder : https://github.com/RAKWireless/RUI_LoRa_node_payload_decoder
 * 
 * @return uint8_t size of data package
 */
uint8_t read_bme680()
{
	bme.performReading();

	double temp = bme.temperature;
	double pres = bme.pressure / 100.0;
	double hum = bme.humidity;
	uint32_t gas = bme.gas_resistance;
	uint16_t batt = 0;
	for (int i = 0; i < 10; i++)
	{
		batt += (uint16_t)(read_batt() / 10);
	}
	batt = batt / 10;

	uint8_t i = 0;
	uint16_t t = temp * 100;
	uint16_t h = hum * 100;
	uint32_t pre = pres * 100;

	collected_data[i++] = 0x01;
	collected_data[i++] = (uint8_t)(t >> 8);
	collected_data[i++] = (uint8_t)t;
	collected_data[i++] = (uint8_t)(h >> 8);
	collected_data[i++] = (uint8_t)h;
	collected_data[i++] = (uint8_t)((pre & 0xFF000000) >> 24);
	collected_data[i++] = (uint8_t)((pre & 0x00FF0000) >> 16);
	collected_data[i++] = (uint8_t)((pre & 0x0000FF00) >> 8);
	collected_data[i++] = (uint8_t)(pre & 0x000000FF);
	collected_data[i++] = (uint8_t)((gas & 0xFF000000) >> 24);
	collected_data[i++] = (uint8_t)((gas & 0x00FF0000) >> 16);
	collected_data[i++] = (uint8_t)((gas & 0x0000FF00) >> 8);
	collected_data[i++] = (uint8_t)(gas & 0x000000FF);
	collected_data[i++] = (uint8_t)(batt >> 8);
	collected_data[i++] = (uint8_t)batt;

	return i;
}
