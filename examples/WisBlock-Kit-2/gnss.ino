/**
 * @file gnss.ino
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief GNSS functions and task
 * @version 0.1
 * @date 2020-07-24
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "app.h"

// The GNSS object
TinyGPSPlus my_gnss;

/** GNSS polling function */
bool poll_gnss(void);

/** Location data as byte array */
tracker_data_s g_tracker_data;

/** Latitude/Longitude value converter */
latLong_s pos_union;

/** Flag if location was found */
bool last_read_ok = false;

/** Flag if GNSS is serial or I2C */
bool i2c_gnss = false;

/**
 * @brief Initialize the GNSS
 * 
 */
bool init_gnss(void)
{
	pinMode(WB_IO2, OUTPUT);
	digitalWrite(WB_IO2, LOW);
	delay(1000);
	digitalWrite(WB_IO2, HIGH);
	// Give the module some time to power up
	delay(2000);

	// // Initialize connection to GPS module
	// Serial1.begin(9600);
	// while (!Serial1)
	//  ;

	/// \todo check if GNSS module is really connected
	return true;
}

/**
 * @brief Check GNSS module for position
 * 
 * @return true Valid position found
 * @return false No valid position
 */
bool poll_gnss(void)
{
	MYLOG("GNSS", "poll_gnss");

	digitalWrite(WB_IO2, HIGH);
	// Give the module some time to power up
	delay(500);

	// Start connection
	Serial1.begin(9600);
	delay(500);

	// delay(100);
	time_t time_out = millis();
	bool has_pos = false;
	bool has_alt = false;
	// bool has_hdop = false;
	int64_t latitude = 0;
	int64_t longitude = 0;
	int32_t altitude = 0;
	uint8_t hdop = 0;

	Serial.println("=============================================");
	while ((millis() - time_out) < 60000)
	{
		while (Serial1.available() > 0)
		{
			char c = Serial1.read();
			// Serial.print(c);
			if (my_gnss.encode(c))
			{
				digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
				if (my_gnss.location.isUpdated() && my_gnss.location.isValid())
				{
					has_pos = true;
					latitude = my_gnss.location.lat() * 10000000;
					longitude = my_gnss.location.lng() * 10000000;
					MYLOG("GNSS", "Lat: %.4f Lon: %.4f\n", latitude / 10000000.0, longitude / 10000000.0);
				}

				// if (my_gnss.hdop.isUpdated() && my_gnss.hdop.isValid())
				// {
				//  has_hdop = true;
				//  double hdop_d = my_gnss.hdop.hdop();
				//  if (g_ble_uart_is_connected)
				//  {
				//    g_ble_uart.printf("hdop = %0.2f\n", hdop_d);
				//  }
				//  hdop = hdop_d;
				// }
				// else if (my_gnss.location.isUpdated() && my_gnss.location.isValid())
				// {
				//  has_pos = true;
				//  latitude = my_gnss.location.lat() * 10000000;
				//  longitude = my_gnss.location.lng() * 10000000;
				//  if (g_ble_uart_is_connected)
				//  {
				//    g_ble_uart.printf("Lat: %.4f Lon: %.4f\n", latitude / 10000000.0, longitude / 10000000.0);
				//  }
				// }
				else if (my_gnss.altitude.isUpdated() && my_gnss.altitude.isValid())
				{
					has_alt = true;
					altitude = my_gnss.altitude.meters() * 1000;
					MYLOG("GNSS", "Alt: %.2f\n", altitude / 1000.0);
					MYLOG("GNSS", "Alt: %.2f\n", my_gnss.altitude.meters());
				}
			}
			// if (has_pos && has_alt && has_hdop)
			if (has_pos && has_alt)
			{
				break;
			}
		}
		// if (has_pos && has_alt && has_hdop)
		if (has_pos && has_alt)
		{
			break;
		}
	}

	Serial.println("\n=============================================");

	// Shut down Serial 1 to save power
	Serial1.end();

	// if (my_gnss.getGnssFixOk())
	if (has_pos && has_alt)
	{
#if MY_DEBUG > 0
		digitalWrite(LED_BLUE, HIGH);
#endif
		/// \todo  For testing, an address in Recife, Brazil, which has both latitude and longitude negative
		// latitude = -80487740;
		// longitude = -349021580;
		// altitude = 156024;

		MYLOG("GNSS", "Fixtype: %d\n", hdop);
		MYLOG("GNSS", "Lat: %.4f Lon: %.4f\n", latitude / 10000000.0, longitude / 10000000.0);
		MYLOG("GNSS", "Alt: %.2f\n", altitude / 1000.0);

		pos_union.val32 = latitude / 1000;
		g_tracker_data.lat_1 = pos_union.val8[2];
		g_tracker_data.lat_2 = pos_union.val8[1];
		g_tracker_data.lat_3 = pos_union.val8[0];

		pos_union.val32 = longitude / 1000;
		g_tracker_data.long_1 = pos_union.val8[2];
		g_tracker_data.long_2 = pos_union.val8[1];
		g_tracker_data.long_3 = pos_union.val8[0];

		pos_union.val32 = altitude / 10;
		g_tracker_data.alt_1 = pos_union.val8[2];
		g_tracker_data.alt_2 = pos_union.val8[1];
		g_tracker_data.alt_3 = pos_union.val8[0];
	}
	else
	{
#if MY_DEBUG > 0
		digitalWrite(LED_BLUE, LOW);
#endif
		MYLOG("GNSS", "No valid location found");
		last_read_ok = false;
		delay(1000);
	}

	// Power down the module
	pinMode(WB_IO2, OUTPUT);
	digitalWrite(WB_IO2, LOW);

	if (has_pos)
	{
		return true;
	}
	return false;
}
