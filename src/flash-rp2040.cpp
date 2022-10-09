/**
 * @file flash-rp2040.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Initialize, read and write parameters from/to internal flash memory
 * @version 0.1
 * @date 2021-01-10
 *
 * @copyright Copyright (c) 2021
 *
 */
#ifdef ARDUINO_ARCH_RP2040

#include "WisBlock-API.h"

s_lorawan_settings g_flash_content;
s_loracompat_settings g_flash_content_compat;

#include <LittleFS_Mbed_RP2040.h>
LittleFS_MBED *myFS;

const char settings_name[] = MBED_LITTLEFS_FILE_PREFIX "/RAK.txt";

FILE *lora_file;

void flash_int_reset(void);

/**
 * @brief Initialize access to RP2040 internal file system
 *
 */
void init_flash(void)
{
	if (init_flash_done)
	{
		return;
	}

	// Initialize Internal File System
	myFS = new LittleFS_MBED();

	if (!myFS->init())
	{
		API_LOG("FLASH", "CRTICIAL: LITTLEFS Mount Failed");

		return;
	}

	// Check if file exists
	lora_file = fopen(settings_name, "r");
	if (!lora_file)
	{
		API_LOG("FLASH", "File doesn't exist, force format");
		delay(100);
		flash_reset();
		lora_file = fopen(settings_name, "r");
	}
	// Found new structure
	fread((uint8_t *)&g_lorawan_settings, 1, sizeof(s_lorawan_settings), lora_file);
	fclose(lora_file);
	// Check if it is LPWAN settings
	if ((g_lorawan_settings.valid_mark_1 != 0xAA) || (g_lorawan_settings.valid_mark_2 != LORAWAN_DATA_MARKER))
	{
		// Data is not valid, reset to defaults
		API_LOG("FLASH", "Invalid data set, deleting and restart node");
		remove(settings_name);
		delay(1000);
		NVIC_SystemReset();
	}
	log_settings();
	init_flash_done = true;
}

/**
 * @brief Save changed settings if required
 *
 * @return boolean
 * 			result of saving
 */
boolean save_settings(void)
{
	bool result = true;
	// Read saved content
	lora_file = fopen(settings_name, "r");
	if (!lora_file)
	{
		API_LOG("FLASH", "File doesn't exist, force format");
		delay(100);
		flash_reset();
		lora_file = fopen(settings_name, "r");
	}
	fread((uint8_t *)&g_flash_content, 1, sizeof(s_lorawan_settings), lora_file);
	fclose(lora_file);
	if (memcmp((void *)&g_flash_content, (void *)&g_lorawan_settings, sizeof(s_lorawan_settings)) != 0)
	{
		API_LOG("FLASH", "Flash content changed, writing new data");
		delay(100);

		remove(settings_name);

		if (lora_file = fopen(settings_name, "w"))
		{
			fwrite((uint8_t *)&g_lorawan_settings, 1, sizeof(s_lorawan_settings), lora_file);
			fflush(lora_file);
		}
		else
		{
			result = false;
		}
		fclose(lora_file);
	}
	log_settings();
	return result;
}

/**
 * @brief Reset content of the filesystem
 *
 */
void flash_reset(void)
{
	API_LOG("FLASH", "flash_reset remove file");
	remove(settings_name);
	delay(1000);

	lora_file = fopen(settings_name, "w");
	if (lora_file)
	{
		s_lorawan_settings default_settings;
		fwrite((uint8_t *)&default_settings, 1, sizeof(s_lorawan_settings), lora_file);
		fflush(lora_file);
		fclose(lora_file);
	}
	else
	{
		API_LOG("FLASH", "flash_reset failed to create file");
	}
}

#endif