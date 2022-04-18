/**
 * @file api_settings.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Application settings
 * @version 0.1
 * @date 2021-09-09
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#include "WisBlock-API.h"

// Define alternate pdMS_TO_TICKS that casts uint64_t for long intervals due to limitation in nrf52840 BSP
#define mypdMS_TO_TICKS(xTimeInMs) ((TickType_t)(((uint64_t)(xTimeInMs)*configTICK_RATE_HZ) / 1000))

#ifdef SW_VERSION_1
uint16_t g_sw_ver_1 = SW_VERSION_1; // major version increase on API change / not backwards compatible
#else
uint16_t g_sw_ver_1 = 1; // major version increase on API change / not backwards compatible
#endif
#ifdef SW_VERSION_2
uint16_t g_sw_ver_2 = SW_VERSION_2; // minor version increase on API change / backward compatible
#else
uint16_t g_sw_ver_2 = 0; // minor version increase on API change / backward compatible
#endif
#ifdef SW_VERSION_3
uint16_t g_sw_ver_3 = SW_VERSION_3; // patch version increase on bugfix, no affect on API
#else
uint16_t g_sw_ver_3 = 0; // patch version increase on bugfix, no affect on API
#endif

// External functions
bool read_rak15001(uint16_t sector, uint8_t *buffer, uint16_t size);
bool write_rak15001(uint16_t sector, uint8_t *buffer, uint16_t size);

/**
 * @brief Set application version
 * 
 * @param sw_1 SW version major number
 * @param sw_2 SW version minor number
 * @param sw_3 SW version patch number
 */
void api_set_version(uint16_t sw_1, uint16_t sw_2, uint16_t sw_3)
{
	g_sw_ver_1 = sw_1;
	g_sw_ver_2 = sw_2;
	g_sw_ver_3 = sw_3;
}

/**
 * @brief Inform API that hard coded LoRaWAN credentials are used
 * 
 */
void api_set_credentials(void)
{
	save_settings();
}

/**
 * @brief Force reading the LoRaWAN credentials
 * 
 */
void api_read_credentials(void)
{
	init_flash();
}

/**
 * @brief System reset
 * 
 */
void api_reset(void)
{
#ifdef NRF52_SERIES
	sd_nvic_SystemReset();
#endif
#ifdef ARDUINO_ARCH_RP2040
	NVIC_SystemReset();
#endif
}

/**
 * @brief Waits for a trigger to wake up
 *    On FreeRTOS the trigger is release of a semaphore
 *    On mbed the trigger is an OS signal
 * 
 */
void api_wait_wake(void)
{
#ifdef NRF52_SERIES
	// Wait until semaphore is released (FreeRTOS)
	xSemaphoreTake(g_task_sem, portMAX_DELAY);
#endif
#ifdef ARDUINO_ARCH_RP2040
	bool got_signal = false;
	while (!got_signal)
	{
		osEvent event = osSignalWait(0, osWaitForever);

		if (event.status == osEventSignal)
		{
			got_signal = true;
		}
	}
#endif
}

/**
 * @brief Wake up loop task
 * 
 * @param reason for wakeup
 */
void api_wake_loop(uint16_t reason)
{
	g_task_event_type |= reason;
	API_LOG("API", "Waking up loop task");

#ifdef NRF52_SERIES
	if (g_task_sem != NULL)
	{
		// Wake up task to send initial packet
		xSemaphoreGive(g_task_sem);
	}
#endif
#ifdef ARDUINO_ARCH_RP2040
	if (loop_thread != NULL)
	{
		osSignalSet(loop_thread, SIGNAL_WAKE);
	}
#endif
}

/**
 * @brief Initialize LoRa transceiver
 * 
 * @return uint32_t result of initialization, 0 = success
 */
uint32_t api_init_lora(void)
{
#ifdef NRF52_SERIES
#ifdef _VARIANT_ISP4520_
	return lora_isp4520_init(SX1262);
#else
	return lora_rak4630_init();
#endif
#endif
#ifdef ARDUINO_ARCH_RP2040
	return lora_rak11300_init();
#endif
}

/**
 * @brief Initialize the timer for frequent sending
 * 
 */
void api_timer_init(void)
{
#ifdef NRF52_SERIES
	// g_task_wakeup_timer.begin(g_lorawan_settings.send_repeat_time, periodic_wakeup);
	g_task_wakeup_timer = xTimerCreate(NULL, mypdMS_TO_TICKS(g_lorawan_settings.send_repeat_time), true, NULL, periodic_wakeup);
#endif
#ifdef ARDUINO_ARCH_RP2040
	g_task_wakeup_timer.oneShot = false;
	g_task_wakeup_timer.ReloadValue = g_lorawan_settings.send_repeat_time;
	TimerInit(&g_task_wakeup_timer, periodic_wakeup);
	TimerSetValue(&g_task_wakeup_timer, g_lorawan_settings.send_repeat_time);
#endif
}

/**
 * @brief Start the timer for frequent sending
 * 
 */
void api_timer_start(void)
{
#ifdef NRF52_SERIES
	// g_task_wakeup_timer.start();
	if (isInISR())
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xTimerChangePeriodFromISR(g_task_wakeup_timer, mypdMS_TO_TICKS(g_lorawan_settings.send_repeat_time), &xHigherPriorityTaskWoken);
		xTimerStartFromISR(g_task_wakeup_timer, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	else
	{
		xTimerChangePeriod(g_task_wakeup_timer, mypdMS_TO_TICKS(g_lorawan_settings.send_repeat_time), 0);
		xTimerStart(g_task_wakeup_timer, 0);
	}
#endif
#ifdef ARDUINO_ARCH_RP2040
	TimerStart(&g_task_wakeup_timer);
#endif
}

/**
 * @brief Stop the timer for frequent sending
 * 
 */
void api_timer_stop(void)
{
#ifdef NRF52_SERIES
	// g_task_wakeup_timer.stop();
	if (isInISR())
	{
		BaseType_t xHigherPriorityTaskWoken = pdFALSE;
		xTimerStopFromISR(g_task_wakeup_timer, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	}
	else
	{
		xTimerStop(g_task_wakeup_timer, 0);
	}

#endif
#ifdef ARDUINO_ARCH_RP2040
	TimerStop(&g_task_wakeup_timer);
#endif
}

/**
 * @brief Restart the time with a new time
 * 
 * @param new_time 
 */
void api_timer_restart(uint32_t new_time)
{
#ifdef NRF52_SERIES
	// g_task_wakeup_timer.stop();
	api_timer_stop();
#endif
#ifdef ARDUINO_ARCH_RP2040
	TimerStop(&g_task_wakeup_timer);
#endif
	if ((g_lorawan_settings.send_repeat_time != 0) && (g_lorawan_settings.auto_join))
	{
#ifdef NRF52_SERIES
		// g_task_wakeup_timer.setPeriod(new_time);
		// g_task_wakeup_timer.start();

		if (isInISR())
		{
			BaseType_t xHigherPriorityTaskWoken = pdFALSE;
			xTimerChangePeriodFromISR(g_task_wakeup_timer, mypdMS_TO_TICKS(new_time), &xHigherPriorityTaskWoken);
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
		else
		{
			xTimerChangePeriod(g_task_wakeup_timer, mypdMS_TO_TICKS(new_time), 0);
		}
#endif
#ifdef ARDUINO_ARCH_RP2040
		TimerSetValue(&g_task_wakeup_timer, new_time);
		TimerStart(&g_task_wakeup_timer);
#endif
	}
}

/**
 * @brief Print current device status over USB
 * 
 */
void api_log_settings(void)
{
	at_settings();
}

/**
 * @brief Read data from a sector of a WisBlock NVRAM Module
 * 		RAK15001 => 
 * 			Sector 0 is reserved for device settings
 * 			Sector size is 4096 bytes max
 *
 * @param sector 
 * 		RAK15001 => Sector to read data from. Valid 1 to 511
 * @param buffer Buffer to write data into
 * @param size Number of bytes
 * @return true if success
 * @return false if failed
 */
bool api_read_ext_nvram(uint16_t sector, uint8_t *buffer, uint16_t size)
{
	if (g_rak15001_flash)
	{
		if (sector == 0)
		{
			API_LOG("API", "RAK15001 Sector 0 is reserved");
			return false;
		}
		return read_rak15001(sector, buffer, size);
	}
	return false;
}

/**
 * @brief Write data into a sector of a WisBlock NVRAM Module
 * 		RAK15001 => 
 * 			Sector 0 is reserved for device settings
 * 			Sector size is 4096 bytes max
 *
 * @param sector 
 * 		RAK15001 => Sector to write data into. Valid 1 to 511
 * @param buffer Buffer with the data to write
 * @param size Number of bytes
 * @return true if success
 * @return false if failed
 */
bool api_write_ext_nvram(uint16_t sector, uint8_t *buffer, uint16_t size)
{
	if (g_rak15001_flash)
	{
		if (sector == 0)
		{
			API_LOG("API", "RAK15001 Sector 0 is reserved");
			return false;
		}
		return write_rak15001(sector, buffer, size);
	}
	return false;
}