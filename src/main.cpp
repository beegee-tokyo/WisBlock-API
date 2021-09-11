/**
 * @file main.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief LoRa configuration over BLE
 * @version 0.1
 * @date 2021-01-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifdef NRF52_SERIES

#include "WisBlock-API.h"

/** Semaphore used by events to wake up loop task */
SemaphoreHandle_t g_task_sem = NULL;

/** Timer to wakeup task frequently and send message */
SoftwareTimer g_task_wakeup_timer;

/** Flag for the event type */
volatile uint16_t g_task_event_type = NO_EVENT;

/** Flag if BLE should be enabled */
bool g_enable_ble = false;

/**
 * @brief Timer event that wakes up the loop task frequently
 * 
 * @param unused 
 */
void periodic_wakeup(TimerHandle_t unused)
{
	// Switch on LED to show we are awake
	digitalWrite(LED_BUILTIN, HIGH);
	g_task_event_type |= STATUS;
	xSemaphoreGiveFromISR(g_task_sem, pdFALSE);
}

/**
 * @brief Arduino setup function. Called once after power-up or reset
 * 
 */
void setup()
{
	// Call app setup for special settings
	setup_app();

	// Create the task event semaphore
	g_task_sem = xSemaphoreCreateBinary();
	// Initialize semaphore
	xSemaphoreGive(g_task_sem);

	// Initialize the built in LED
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	// Initialize the connection status LED
	pinMode(LED_CONN, OUTPUT);
	digitalWrite(LED_CONN, HIGH);

#if MY_DEBUG > 0
	// Initialize Serial for debug output
	Serial.begin(115200);

	time_t serial_timeout = millis();
	// On nRF52840 the USB serial is not available immediately
	while (!Serial)
	{
		if ((millis() - serial_timeout) < 5000)
		{
			delay(100);
			digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
		}
		else
		{
			break;
		}
	}
#endif

	digitalWrite(LED_BUILTIN, HIGH);

	MYLOG("MAIN", "=====================================");
	MYLOG("MAIN", "RAK4631 LoRaWAN GNSS Tracker");
	MYLOG("MAIN", "=====================================");

	// Initialize battery reading
	init_batt();

	// Get LoRa parameter
	init_flash();

	if (g_enable_ble)
	{
		// Init BLE
		init_ble();
	}
	else
	{
		// BLE is not activated, switch off blue LED
		digitalWrite(LED_CONN, LOW);
	}

	// Take the semaphore so the loop will go to sleep until an event happens
	xSemaphoreTake(g_task_sem, 10);

	// Check if auto join is enabled
	if (g_lorawan_settings.auto_join)
	{
		MYLOG("MAIN", "Auto join is enabled, start LoRa and join");
		// Initialize LoRa and start join request
		int8_t lora_init_result = init_lora();

		if (lora_init_result != 0)
		{
			MYLOG("MAIN", "Init LoRa failed");

			// Without working LoRa we just stop here
			while (1)
			{
				MYLOG("MAIN", "Get your LoRa stuff in order");
				pinMode(LED_BUILTIN, OUTPUT);
				digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
				delay(5000);
			}
		}
		MYLOG("MAIN", "LoRa init success");
	}
	else
	{
		// Put radio into sleep mode
		lora_rak4630_init();
		Radio.Sleep();

		MYLOG("MAIN", "Auto join is disabled, waiting for connect command");
		delay(100);
	}

	// Initialize application
	if (!init_app())
	{
		// Without working LoRa we just stop here
		while (1)
		{
			MYLOG("MAIN", "Get your application stuff in order");
			pinMode(LED_BUILTIN, OUTPUT);
			digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
			delay(5000);
		}
	}

}

/**
 * @brief Arduino loop task. Called in a loop from the FreeRTOS task handler
 * 
 */
void loop()
{
	// Sleep until we are woken up by an event
	if (xSemaphoreTake(g_task_sem, portMAX_DELAY) == pdTRUE)
	{
		// Switch on green LED to show we are awake
		digitalWrite(LED_BUILTIN, HIGH);
		while (g_task_event_type != NO_EVENT)
		{
			// Application specific event handler (timer event or others)
			app_event_handler();

			if (ble_data_handler != NULL)
			{
				// Handle BLE UART events
				ble_data_handler();
			}

			// Handle LoRa data events
			lora_data_handler();

			// Handle BLE configuration event
			if ((g_task_event_type & BLE_CONFIG) == BLE_CONFIG)
			{
				g_task_event_type &= N_BLE_CONFIG;
				MYLOG("MAIN", "Config received over BLE");
				delay(100);

				// Inform connected device about new settings
				g_lora_data.write((void *)&g_lorawan_settings, sizeof(s_lorawan_settings));
				g_lora_data.notify((void *)&g_lorawan_settings, sizeof(s_lorawan_settings));

				// Check if auto connect is enabled
				if ((g_lorawan_settings.auto_join) && !g_lorawan_initialized)
				{
					init_lora();
				}
			}

			// Serial input event
			if ((g_task_event_type & AT_CMD) == AT_CMD)
			{
				g_task_event_type &= N_AT_CMD;
				MYLOG("APP", "USB data received");

				while (Serial.available() > 0)
				{
					at_serial_input(uint8_t(Serial.read()));
					delay(5);
				}
			}
		}
		MYLOG("MAIN", "Loop goes to sleep");
		g_task_event_type = 0;
		// Go back to sleep
		xSemaphoreTake(g_task_sem, 10);
		// Switch off blue LED to show we go to sleep
		digitalWrite(LED_BUILTIN, LOW);
		delay(10);
	}
}

#endif