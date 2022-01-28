/**
 * @file lis3dh_acc.ino
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Function to handle the LIS3DH Acceleration sensor
 * @version 0.1
 * @date 2021-09-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <Arduino.h>
#include <SparkFunLIS3DH.h> //http://librarymanager/All#SparkFun-LIS3DH

void acc_int_handler(void);

/** The LIS3DH sensor */
LIS3DH acc_sensor(I2C_MODE, 0x18);

/** Flag if a x-axis movement was detected */
bool has_x_move = false;
/** Flag if a y-axis movement was detected */
bool has_y_move = false;
/** Flag if a z-axis movement was detected */
bool has_z_move = false;

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
	pinMode(WB_IO5, INPUT);

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

	uint8_t dataToWrite = 0;
	dataToWrite |= 0x20;									//Z high
	dataToWrite |= 0x08;									//Y high
	dataToWrite |= 0x02;									//X high
	acc_sensor.writeRegister(LIS3DH_INT1_CFG, dataToWrite); // Enable interrupts on high tresholds for x, y and z

	dataToWrite = 0;
	dataToWrite |= 0x10;									// 1/8 range
	acc_sensor.writeRegister(LIS3DH_INT1_THS, dataToWrite); // 1/8th range

	dataToWrite = 0;
	dataToWrite |= 0x01; // 1 * 1/50 s = 20ms
	acc_sensor.writeRegister(LIS3DH_INT1_DURATION, dataToWrite);

	acc_sensor.readRegister(&dataToWrite, LIS3DH_CTRL_REG5);
	dataToWrite &= 0xF3;									 //Clear bits of interest
	dataToWrite |= 0x08;									 //Latch interrupt (Cleared by reading int1_src)
	acc_sensor.writeRegister(LIS3DH_CTRL_REG5, dataToWrite); // Set interrupt to latching

	dataToWrite = 0;
	dataToWrite |= 0x40; //AOI1 event (Generator 1 interrupt on pin 1)
	dataToWrite |= 0x20; //AOI2 event ()
	acc_sensor.writeRegister(LIS3DH_CTRL_REG3, dataToWrite);

	acc_sensor.writeRegister(LIS3DH_CTRL_REG6, 0x00); // No interrupt on pin 2

	acc_sensor.writeRegister(LIS3DH_CTRL_REG2, 0x01); // Enable high pass filter

	// Set low power mode
	dataToWrite = 0;
	acc_sensor.readRegister(&dataToWrite, LIS3DH_CTRL_REG1);
	dataToWrite |= 0x08;
	acc_sensor.writeRegister(LIS3DH_CTRL_REG1, dataToWrite);
	delay(100);

	dataToWrite = 0;
	acc_sensor.readRegister(&dataToWrite, 0x1E);
	dataToWrite |= 0x90;
	acc_sensor.writeRegister(0x1E, dataToWrite);
	delay(100);

	get_acc_int();

	// Set the interrupt callback function
	attachInterrupt(WB_IO5, acc_int_handler, RISING);

	return true;
}

/**
 * @brief ACC interrupt handler
 * @note gives semaphore to wake up main loop
 * 
 */
void acc_int_handler(void)
{
	// Wake up the task to handle it
	api_wake_loop(ACC_TRIGGER);
}

/**
 * @brief Reading ACC X, Y and Z values
 * 
 * @param acc_values Array of int16_t to hold the ACC values
 */
void read_acc_vals(int16_t *acc_values)
{
	acc_values[0] = (int16_t)(acc_sensor.readFloatAccelX() * 1000.0);
	acc_values[1] = (int16_t)(acc_sensor.readFloatAccelY() * 1000.0);
	acc_values[2] = (int16_t)(acc_sensor.readFloatAccelZ() * 1000.0);

	MYLOG("ACC", "X %.3f %.3f %d", acc_sensor.readFloatAccelX(), acc_sensor.readFloatAccelX() * 1000.0, acc_values[0]);
	MYLOG("ACC", "Y %.3f %.3f %d", acc_sensor.readFloatAccelY(), acc_sensor.readFloatAccelY() * 1000.0, acc_values[1]);
	MYLOG("ACC", "Z %.3f %.3f %d", acc_sensor.readFloatAccelZ(), acc_sensor.readFloatAccelZ() * 1000.0, acc_values[2]);
}

/**
 * @brief Read ACC interrupt register and clear it to enable next wakeup
 * 
 */
void get_acc_int(void)
{
	uint8_t dataRead;
	acc_sensor.readRegister(&dataRead, LIS3DH_INT1_SRC);

	if (dataRead & 0x40)
		MYLOG("ACC", "Interrupt Active 0x%X", dataRead);
	if (dataRead & 0x20)
	{
		MYLOG("ACC", "Z high");
		has_z_move = true;
	}
	if (dataRead & 0x10)
	{
		MYLOG("ACC", "Z low");
		has_z_move = false;
	}
	if (dataRead & 0x08)
	{
		MYLOG("ACC", "Y high");
		has_y_move = true;
	}
	if (dataRead & 0x04)
	{
		MYLOG("ACC", "Y low");
		has_y_move = false;
	}
	if (dataRead & 0x02)
	{
		MYLOG("ACC", "X high");
		has_x_move = true;
	}
	if (dataRead & 0x01)
	{
		MYLOG("ACC", "X low");
		has_x_move = false;
	}
}
