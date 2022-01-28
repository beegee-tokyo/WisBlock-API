/**
 * @file lis3dh_acc.ino
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief 3-axis accelerometer functions
 * @version 0.1
 * @date 2020-07-24
 * 
 * @copyright Copyright (c) 2020
 * 
 */
// #include "app.h"
#include <Arduino.h>
#include <SparkFunLIS3DH.h> //http://librarymanager/All#SparkFun-LIS3DH

#define INT1_PIN WB_IO3

void acc_int_callback(void);
uint16_t *read_acc(void);

/** The LIS3DH sensor */
LIS3DH acc_sensor(I2C_MODE, 0x18);

/**
 * @brief Initialize LIS3DH 3-axis 
 * acceleration sensor
 * 
 * @return true If sensor was found and is initialized
 * @return false If sensor initialization failed
 */
bool init_acc(void)
{
	// Setup interrupt pin
	pinMode(INT1_PIN, INPUT);

	Wire.begin();

	acc_sensor.settings.accelSampleRate = 10; //Hz.  Can be: 0,1,10,25,50,100,200,400,1600,5000 Hz
	acc_sensor.settings.accelRange = 2;		  //Max G force readable.  Can be: 2, 4, 8, 16

	acc_sensor.settings.adcEnabled = 0;
	acc_sensor.settings.tempEnabled = 0;
	acc_sensor.settings.xAccelEnabled = 1;
	acc_sensor.settings.yAccelEnabled = 1;
	acc_sensor.settings.zAccelEnabled = 1;

	if (acc_sensor.begin() != 0)
	{
		return false;
	}

	uint8_t data_to_write = 0;
	//************************************************************************/
	// Below functions can be used to enable the ACC's IRQ and trigger
	// Sending a packet on movement.
	//************************************************************************/
	// // Enable interrupts
	// data_to_write |= 0x20;                   //Z high
	// data_to_write |= 0x08;                   //Y high
	// data_to_write |= 0x02;                   //X high
	// acc_sensor.writeRegister(LIS3DH_INT1_CFG, data_to_write); // Enable interrupts on high tresholds for x, y and z

	// // Set interrupt trigger range
	// data_to_write = 0;
	// // data_to_write |= 0x10;                    // 1/8 range
	// // acc_sensor.writeRegister(LIS3DH_INT1_THS, data_to_write); // 1/8th range
	// data_to_write |= 0x08;                   // 1/16 range
	// acc_sensor.writeRegister(LIS3DH_INT1_THS, data_to_write); // 1/16th range

	// // Set interrupt signal length
	// data_to_write = 0;
	// data_to_write |= 0x01; // 1 * 1/50 s = 20ms
	// acc_sensor.writeRegister(LIS3DH_INT1_DURATION, data_to_write);

	// acc_sensor.readRegister(&data_to_write, LIS3DH_CTRL_REG5);
	// data_to_write &= 0xF3;                    //Clear bits of interest
	// data_to_write |= 0x08;                    //Latch interrupt (Cleared by reading int1_src)
	// acc_sensor.writeRegister(LIS3DH_CTRL_REG5, data_to_write); // Set interrupt to latching

	// // Select interrupt pin 1
	// data_to_write = 0;
	// data_to_write |= 0x40; //AOI1 event (Generator 1 interrupt on pin 1)
	// data_to_write |= 0x20; //AOI2 event ()
	// acc_sensor.writeRegister(LIS3DH_CTRL_REG3, data_to_write);

	// // No interrupt on pin 2
	// acc_sensor.writeRegister(LIS3DH_CTRL_REG6, 0x00);

	// Enable high pass filter
	acc_sensor.writeRegister(LIS3DH_CTRL_REG2, 0x01);

	// Set low power mode
	data_to_write = 0;
	acc_sensor.readRegister(&data_to_write, LIS3DH_CTRL_REG1);
	data_to_write |= 0x08;
	acc_sensor.writeRegister(LIS3DH_CTRL_REG1, data_to_write);
	delay(100);

	data_to_write = 0;
	acc_sensor.readRegister(&data_to_write, 0x1E);
	data_to_write |= 0x90;
	acc_sensor.writeRegister(0x1E, data_to_write);
	delay(100);

	//************************************************************************/
	// Below functions can be used to enable the ACC's IRQ and trigger
	// Sending a packet on movement.
	//************************************************************************/
	// clear_acc_int();

	// // Set the interrupt callback function
	// attachInterrupt(INT1_PIN, acc_int_callback, RISING);

	read_acc();

	return true;
}

uint16_t acc[3] = {0};

uint16_t *read_acc(void)
{
	acc[0] = (int16_t)(acc_sensor.readFloatAccelX() * 1000.0);
	acc[1] = (int16_t)(acc_sensor.readFloatAccelY() * 1000.0);
	acc[2] = (int16_t)(acc_sensor.readFloatAccelZ() * 1000.0);
	return acc;
}

/**
 * @brief Clear ACC interrupt register to enable next wakeup
 * 
 */
void clear_acc_int(void)
{
	uint8_t data_read;
	acc_sensor.readRegister(&data_read, LIS3DH_INT1_SRC);
}
