/**
   @file app.cpp
   @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
   @brief Application specific functions. Mandatory to have init_app(),
          app_event_handler(), ble_data_handler(), lora_data_handler()
          and lora_tx_finished()
   @version 0.1
   @date 2021-04-23

   @copyright Copyright (c) 2021

*/

#include "app.h"

/** Set the device name, max length is 10 characters */
char g_ble_dev_name[10] = "RAK-TEST";

/** Send Fail counter **/
uint8_t send_fail = 0;

/** Flag for low battery protection */
bool low_batt_protection = false;

/** Battery level uinion */
batt_s batt_level;

/** LPWAN packet */
lpwan_data_s g_lpwan_data;

/**
   @brief Application specific setup functions

*/
void setup_app(void)
{
	pinMode(LED_BLUE, OUTPUT);
#if API_DEBUG == 0
	// Initialize Serial for debug output
	Serial.begin(115200);

	time_t serial_timeout = millis();
	// On nRF52840 the USB serial is not available immediately
	while (!Serial)
	{
		if ((millis() - serial_timeout) < 1000)
		{
			delay(100);
			digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
		}
		else
		{
			break;
		}
	}
#endif
#ifdef NRF52_SERIES
	// Enable BLE
	g_enable_ble = true;
#endif
}

/**
   @brief Application specific initializations

   @return true Initialization success
   @return false Initialization failure
*/
bool init_app(void)
{
	MYLOG("APP", "init_app");

	return true;
}

/**
   @brief Application specific event handler
          Requires as minimum the handling of STATUS event
          Here you handle as well your application specific events
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
			if (g_lorawan_settings.auto_join)
			{
				restart_advertising(60);
			}
		}
#endif

		if (!low_batt_protection)
		{
			// Sensor readings here
		}

		// Get battery level
		batt_level.batt16 = read_batt() / 10;
		g_lpwan_data.batt_1 = batt_level.batt8[1];
		g_lpwan_data.batt_2 = batt_level.batt8[0];

		// Protection against battery drain
		if (batt_level.batt16 < 290)
		{
			// Battery is very low, change send time to 1 hour to protect battery
			low_batt_protection = true;			   // Set low_batt_protection active
			api_timer_restart(1 * 60 * 60 * 1000); // Set send time to one hour
			MYLOG("APP", "Battery protection activated");
		}
		else if ((batt_level.batt16 > 380) && low_batt_protection)
		{
			// Battery is higher than 4V, change send time back to original setting
			low_batt_protection = false;
			api_timer_restart(g_lorawan_settings.send_repeat_time);
			MYLOG("APP", "Battery protection deactivated");
		}

		if (!send_p2p_packet((uint8_t *)&g_lpwan_data, LPWAN_DATA_LEN))
		{
			MYLOG("APP", "Failed to send P2P packet");
		}
	}
}

#ifdef NRF52_SERIES
/**
   @brief Handle BLE UART data

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
   @brief Handle received LoRa Data

*/
void lora_data_handler(void)
{
	// LoRa TX finished handling
	if ((g_task_event_type & LORA_TX_FIN) == LORA_TX_FIN)
	{
		g_task_event_type &= N_LORA_TX_FIN;

		MYLOG("APP", "LPWAN TX cycle %s", g_rx_fin_result ? "finished ACK" : "failed NAK");
	}

	// LoRa data handling
	if ((g_task_event_type & LORA_DATA) == LORA_DATA)
	{
		digitalWrite(LED_BLUE, !digitalRead(LED_BLUE));
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

		MYLOG("APP", "%s", log_buff);
	}
}
