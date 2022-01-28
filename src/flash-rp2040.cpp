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

void flash_reset(void);

bool init_flash_done = false;

/**
 * @brief Initialize access to nRF52 internal file system
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

/**
 * @brief Printout of all settings
 * 
 */
void log_settings(void)
{
	API_LOG("FLASH", "Saved settings:");
	API_LOG("FLASH", "000 Marks: %02X %02X", g_lorawan_settings.valid_mark_1, g_lorawan_settings.valid_mark_2);
	API_LOG("FLASH", "002 Dev EUI %02X%02X%02X%02X%02X%02X%02X%02X", g_lorawan_settings.node_device_eui[0], g_lorawan_settings.node_device_eui[1],
			g_lorawan_settings.node_device_eui[2], g_lorawan_settings.node_device_eui[3],
			g_lorawan_settings.node_device_eui[4], g_lorawan_settings.node_device_eui[5],
			g_lorawan_settings.node_device_eui[6], g_lorawan_settings.node_device_eui[7]);
	API_LOG("FLASH", "010 App EUI %02X%02X%02X%02X%02X%02X%02X%02X", g_lorawan_settings.node_app_eui[0], g_lorawan_settings.node_app_eui[1],
			g_lorawan_settings.node_app_eui[2], g_lorawan_settings.node_app_eui[3],
			g_lorawan_settings.node_app_eui[4], g_lorawan_settings.node_app_eui[5],
			g_lorawan_settings.node_app_eui[6], g_lorawan_settings.node_app_eui[7]);
	API_LOG("FLASH", "018 App Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			g_lorawan_settings.node_app_key[0], g_lorawan_settings.node_app_key[1],
			g_lorawan_settings.node_app_key[2], g_lorawan_settings.node_app_key[3],
			g_lorawan_settings.node_app_key[4], g_lorawan_settings.node_app_key[5],
			g_lorawan_settings.node_app_key[6], g_lorawan_settings.node_app_key[7],
			g_lorawan_settings.node_app_key[8], g_lorawan_settings.node_app_key[9],
			g_lorawan_settings.node_app_key[10], g_lorawan_settings.node_app_key[11],
			g_lorawan_settings.node_app_key[12], g_lorawan_settings.node_app_key[13],
			g_lorawan_settings.node_app_key[14], g_lorawan_settings.node_app_key[15]);
	API_LOG("FLASH", "034 Dev Addr %08lX", g_lorawan_settings.node_dev_addr);
	API_LOG("FLASH", "038 NWS Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			g_lorawan_settings.node_nws_key[0], g_lorawan_settings.node_nws_key[1],
			g_lorawan_settings.node_nws_key[2], g_lorawan_settings.node_nws_key[3],
			g_lorawan_settings.node_nws_key[4], g_lorawan_settings.node_nws_key[5],
			g_lorawan_settings.node_nws_key[6], g_lorawan_settings.node_nws_key[7],
			g_lorawan_settings.node_nws_key[8], g_lorawan_settings.node_nws_key[9],
			g_lorawan_settings.node_nws_key[10], g_lorawan_settings.node_nws_key[11],
			g_lorawan_settings.node_nws_key[12], g_lorawan_settings.node_nws_key[13],
			g_lorawan_settings.node_nws_key[14], g_lorawan_settings.node_nws_key[15]);
	API_LOG("FLASH", "054 Apps Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			g_lorawan_settings.node_apps_key[0], g_lorawan_settings.node_apps_key[1],
			g_lorawan_settings.node_apps_key[2], g_lorawan_settings.node_apps_key[3],
			g_lorawan_settings.node_apps_key[4], g_lorawan_settings.node_apps_key[5],
			g_lorawan_settings.node_apps_key[6], g_lorawan_settings.node_apps_key[7],
			g_lorawan_settings.node_apps_key[8], g_lorawan_settings.node_apps_key[9],
			g_lorawan_settings.node_apps_key[10], g_lorawan_settings.node_apps_key[11],
			g_lorawan_settings.node_apps_key[12], g_lorawan_settings.node_apps_key[13],
			g_lorawan_settings.node_apps_key[14], g_lorawan_settings.node_apps_key[15]);
	API_LOG("FLASH", "070 OTAA %s", g_lorawan_settings.otaa_enabled ? "enabled" : "disabled");
	API_LOG("FLASH", "071 ADR %s", g_lorawan_settings.adr_enabled ? "enabled" : "disabled");
	API_LOG("FLASH", "072 %s Network", g_lorawan_settings.public_network ? "Public" : "Private");
	API_LOG("FLASH", "073 Dutycycle %s", g_lorawan_settings.duty_cycle_enabled ? "enabled" : "disabled");
	API_LOG("FLASH", "074 Repeat time %ld", g_lorawan_settings.send_repeat_time);
	API_LOG("FLASH", "078 Join trials %d", g_lorawan_settings.join_trials);
	API_LOG("FLASH", "079 TX Power %d", g_lorawan_settings.tx_power);
	API_LOG("FLASH", "080 DR %d", g_lorawan_settings.data_rate);
	API_LOG("FLASH", "081 Class %d", g_lorawan_settings.lora_class);
	API_LOG("FLASH", "082 Subband %d", g_lorawan_settings.subband_channels);
	API_LOG("FLASH", "083 Auto join %s", g_lorawan_settings.auto_join ? "enabled" : "disabled");
	API_LOG("FLASH", "084 Fport %d", g_lorawan_settings.app_port);
	API_LOG("FLASH", "085 %s Message", g_lorawan_settings.confirmed_msg_enabled ? "Confirmed" : "Unconfirmed");
	API_LOG("FLASH", "086 Region %s", region_names[g_lorawan_settings.lora_region]);
	API_LOG("FLASH", "087 Mode %s", g_lorawan_settings.lorawan_enable ? "LPWAN" : "P2P");
	API_LOG("FLASH", "088 P2P frequency %ld", g_lorawan_settings.p2p_frequency);
	API_LOG("FLASH", "092 P2P TX Power %d", g_lorawan_settings.p2p_tx_power);
	API_LOG("FLASH", "093 P2P BW %d", g_lorawan_settings.p2p_bandwidth);
	API_LOG("FLASH", "094 P2P SF %d", g_lorawan_settings.p2p_sf);
	API_LOG("FLASH", "095 P2P CR %d", g_lorawan_settings.p2p_cr);
	API_LOG("FLASH", "096 P2P Preamble length %d", g_lorawan_settings.p2p_preamble_len);
	API_LOG("FLASH", "097 P2P Symbol Timeout %d", g_lorawan_settings.p2p_symbol_timeout);
}

#endif