/**
 * @file WisBlock-Kit-2.ino
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Example for a GNSS tracker based on WisBlock modules
 * @version 0.1
 * @date 2021-09-11
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "app.h"

#ifdef NRF52_SERIES
/** Timer to wakeup task frequently and send message */
SoftwareTimer delayed_sending;
#endif
#ifdef ARDUINO_ARCH_RP2040
/** Timer for periodic sending */
TimerEvent_t delayed_sending;
#endif

/** Set the device name, max length is 10 characters */
char g_ble_dev_name[10] = "RAK-GNSS";

/** Flag showing if TX cycle is ongoing */
bool lora_busy = false;

/** Timer since last position message was sent */
time_t last_pos_send = 0;
/** Timer for delayed sending to keep duty cycle */

/** Battery level uinion */
batt_s batt_level;

/** Flag if delayed sending is already activated */
bool delayed_active = false;

/** Minimum delay between sending new locations, set to 45 seconds */
time_t min_delay = 45000;

/** Flag if BME680 was found */
bool has_env = false;

#ifdef NRF52_SERIES
/**
 * @brief Timer function used to avoid sending packages too often.
 *       Delays the next package by 10 seconds
 * 
 * @param unused 
 *      Timer handle, not used
 */
void send_delayed(TimerHandle_t unused)
{
	api_wake_loop(STATUS);
}
#endif
#ifdef ARDUINO_ARCH_RP2040
/**
 * @brief Timer function used to avoid sending packages too often.
 *      Delays the next package by 10 seconds
 * 
 */
void send_delayed(void)
{
	api_wake_loop(STATUS);
}
#endif

/**
 * @brief Application specific setup functions
 * 
 */
void setup_app(void)
{
	Serial.begin(115200);
	time_t serial_timeout = millis();
	// On nRF52840 the USB serial is not available immediately
	while (!Serial)
	{
		if ((millis() - serial_timeout) < 5000)
		{
			delay(100);
			digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
		}
		else
		{
			break;
		}
	}
	digitalWrite(LED_GREEN, LOW);

	MYLOG("APP", "Setup WisBlock Kit 2 Example");

#ifdef NRF52_SERIES
	// Enable BLE
	g_enable_ble = true;
#endif
}

/**
 * @brief Application specific initializations
 * 
 * @return true Initialization success
 * @return false Initialization failure
 */
bool init_app(void)
{
	bool init_result = true;
	MYLOG("APP", "init_app");

	pinMode(WB_IO2, OUTPUT);
	digitalWrite(WB_IO2, HIGH);

	// Start the I2C bus
	Wire.begin();
	Wire.setClock(400000);

	// Initialize GNSS module
	MYLOG("APP", "Initialize uBlox GNSS");
	init_result = init_gnss();
	MYLOG("APP", "Result %s", init_result ? "success" : "failed");

	// Initialize Temperature sensor
	MYLOG("APP", "Initialize BME680");
	has_env = init_bme();
	MYLOG("APP", "Result %s", has_env ? "success" : "failed");

	// Initialize ACC sensor
	MYLOG("APP", "Initialize Accelerometer");

	if (!init_acc())
	{
		MYLOG("ACC", "ACC sensor initialization failed");
		init_result |= false;
	}
	MYLOG("APP", "Result %s", init_result ? "success" : "failed");

	if (g_lorawan_settings.send_repeat_time != 0)
	{
		// Set delay for sending to scheduled sending time
		min_delay = g_lorawan_settings.send_repeat_time / 2;
	}
	else
	{
		// Send repeat time is 0, set delay to 30 seconds
		min_delay = 30000;
	}
	// Set to 1/2 of programmed send interval or 30 seconds
#ifdef NRF52_SERIES
	delayed_sending.begin(g_lorawan_settings.send_repeat_time, send_delayed);
#endif
#ifdef ARDUINO_ARCH_RP2040
	delayed_sending.oneShot = false;
	delayed_sending.ReloadValue = g_lorawan_settings.send_repeat_time;
	TimerInit(&delayed_sending, send_delayed);
	TimerSetValue(&delayed_sending, g_lorawan_settings.send_repeat_time);
#endif

	// Power down GNSS module
	// pinMode(WB_IO2, OUTPUT);
	// digitalWrite(WB_IO2, LOW);
	return init_result;
}

/**
 * @brief Application specific event handler
 *        Requires as minimum the handling of STATUS event
 *        Here you handle as well your application specific events
 */
void app_event_handler(void)
{
	// Timer triggered event
	if ((g_task_event_type & STATUS) == STATUS)
	{
		g_task_event_type &= N_STATUS;
		MYLOG("APP", "Timer wakeup");

#ifdef NRF52_SERIES
		// If BLE is enabled, restart Advertising
		if (g_enable_ble)
		{
			restart_advertising(15);
		}
#endif
		if (lora_busy)
		{
			MYLOG("APP", "LoRaWAN TX cycle not finished, skip this event");
		}
		else
		{
			// Wake up the temperature sensor and start measurements
			// wake_th();
			start_bme();

			if (poll_gnss())
			{
				MYLOG("APP", "Valid GNSS position");
			}
			else
			{
				MYLOG("APP", "No valid GNSS position");
			}

			if (has_env)
			{
				// Get temperature and humidity data
				read_bme();
			}

			// Get ACC values
			uint16_t *acc = read_acc();

			g_tracker_data.acc_x_1 = (int8_t)(acc[0] >> 8);
			g_tracker_data.acc_x_2 = (int8_t)(acc[0]);
			g_tracker_data.acc_y_1 = (int8_t)(acc[1] >> 8);
			g_tracker_data.acc_y_2 = (int8_t)(acc[1]);
			g_tracker_data.acc_z_1 = (int8_t)(acc[2] >> 8);
			g_tracker_data.acc_z_2 = (int8_t)(acc[2]);

			// Get battery level
			// g_tracker_data.batt = mv_to_percent(read_batt());
			batt_level.batt16 = read_batt() / 10;
			g_tracker_data.batt_1 = batt_level.batt8[1];
			g_tracker_data.batt_2 = batt_level.batt8[0];

			// Remember last time sending
			last_pos_send = millis();
			// Just in case
			delayed_active = false;

#if MY_DEBUG == 1
			uint8_t *packet = &g_tracker_data.data_flag1;
			for (int idx = 0; idx < TRACKER_DATA_LEN; idx++)
			{
				Serial.printf("%02X", packet[idx]);
			}
			Serial.println("");
#endif
			lmh_error_status result;

			if (has_env)
			{
				// Tracker has BME680, send full packet
				result = send_lora_packet((uint8_t *)&g_tracker_data, TRACKER_DATA_LEN);
			}
			else
			{
				// Tracker has no BME680, send short packet
				result = send_lora_packet((uint8_t *)&g_tracker_data, TRACKER_DATA_SHORT_LEN);
			}
			switch (result)
			{
			case LMH_SUCCESS:
				MYLOG("APP", "Packet enqueued");
				/// \todo set a flag that TX cycle is running
				lora_busy = true;
				break;
			case LMH_BUSY:
				MYLOG("APP", "LoRa transceiver is busy");
				break;
			case LMH_ERROR:
				MYLOG("APP", "Packet error, too big to send with current DR");
				break;
			}
		}
	}

	// ACC trigger event
	if ((g_task_event_type & ACC_TRIGGER) == ACC_TRIGGER)
	{
		g_task_event_type &= N_ACC_TRIGGER;
		MYLOG("APP", "ACC triggered");

		// Check time since last send
		bool send_now = true;
		if (g_lorawan_settings.send_repeat_time != 0)
		{
			if ((millis() - last_pos_send) < min_delay)
			{
				send_now = false;
				if (!delayed_active)
				{
					// delayed_sending.stop();
#ifdef NRF52_SERIES
					delayed_sending.stop();
#endif
#ifdef ARDUINO_ARCH_RP2040
					TimerStop(&delayed_sending);
#endif
					MYLOG("APP", "Expired time %d", (int)(millis() - last_pos_send));
					MYLOG("APP", "Max delay time %d", (int)min_delay);

					time_t wait_time = abs(min_delay - (millis() - last_pos_send) >= 0) ? (min_delay - (millis() - last_pos_send)) : min_delay;
					MYLOG("APP", "Wait time %ld", (long)wait_time);

					MYLOG("APP", "Only %lds since last position message, send delayed in %lds", (long)((millis() - last_pos_send) / 1000), (long)(wait_time / 1000));

#ifdef NRF52_SERIES
					delayed_sending.setPeriod(wait_time);
					delayed_sending.start();
#endif
#ifdef ARDUINO_ARCH_RP2040
					TimerSetValue(&delayed_sending, wait_time);
					TimerStart(&delayed_sending);
#endif

					delayed_active = true;
				}
			}
		}
		if (send_now)
		{
			// Remember last send time
			last_pos_send = millis();

			// Trigger a GNSS reading and packet sending
			g_task_event_type |= STATUS;
		}

		// Reset the standard timer
		if (g_lorawan_settings.send_repeat_time != 0)
		{
			api_timer_restart(g_lorawan_settings.send_repeat_time);
		}
	}
}

#ifdef NRF52_SERIES
/**
 * @brief Handle BLE UART data
 * 
 */
void ble_data_handler(void)
{
	if (g_enable_ble)
	{
		// BLE UART data handling
		if ((g_task_event_type & BLE_DATA) == BLE_DATA)
		{
			MYLOG("AT", "RECEIVED BLE");
			/** BLE UART data arrived */
			g_task_event_type &= N_BLE_DATA;

			while (g_ble_uart.available() > 0)
			{
				at_serial_input(uint8_t(g_ble_uart.read()));
				delay(5);
			}
			at_serial_input(uint8_t('\n'));
		}
	}
}
#endif

/**
 * @brief Handle received LoRa Data
 * 
 */
void lora_data_handler(void)
{
	// LoRa Join finished handling
	if ((g_task_event_type & LORA_JOIN_FIN) == LORA_JOIN_FIN)
	{
		g_task_event_type &= N_LORA_JOIN_FIN;
		if (g_join_result)
		{
			MYLOG("APP", "Successfully joined network");
		}
		else
		{
			MYLOG("APP", "Join network failed");
			/// \todo here join could be restarted.
			// lmh_join();
		}
	}

	// LoRa data handling
	if ((g_task_event_type & LORA_DATA) == LORA_DATA)
	{
		/**************************************************************/
		/**************************************************************/
		/// \todo LoRa data arrived
		/// \todo parse them here
		/**************************************************************/
		/**************************************************************/
		g_task_event_type &= N_LORA_DATA;
		MYLOG("APP", "Received package over LoRa");
		char log_buff[g_rx_data_len * 3] = {0};
		uint8_t log_idx = 0;
		for (int idx = 0; idx < g_rx_data_len; idx++)
		{
			sprintf(&log_buff[log_idx], "%02X ", g_rx_lora_data[idx]);
			log_idx += 3;
		}
		lora_busy = false;
		MYLOG("APP", "%s", log_buff);
	}

	// LoRa TX finished handling
	if ((g_task_event_type & LORA_TX_FIN) == LORA_TX_FIN)
	{
		g_task_event_type &= N_LORA_TX_FIN;

		MYLOG("APP", "LPWAN TX cycle %s", g_rx_fin_result ? "finished ACK" : "failed NAK");

		/// \todo reset flag that TX cycle is running
		lora_busy = false;
	}
}

/**
 * @brief ACC interrupt handler
 * @note gives semaphore to wake up main loop
 * 
 */
void acc_int_callback(void)
{
	api_wake_loop(ACC_TRIGGER);
}
