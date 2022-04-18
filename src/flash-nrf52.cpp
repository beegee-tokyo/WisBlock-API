/**
 * @file flash-nrf52.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Initialize, read and write parameters from/to internal flash memory
 * @version 0.1
 * @date 2021-01-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifdef NRF52_SERIES

#include "WisBlock-API.h"

s_lorawan_settings g_flash_content;
s_loracompat_settings g_flash_content_compat;

#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>
using namespace Adafruit_LittleFS_Namespace;

const char settings_name[] = "RAK";

File lora_file(InternalFS);

void flash_int_reset(void);

/**
 * @brief Initialize access to nRF52 internal file system
 * 
 */
void init_int_flash(void)
{
	if (init_flash_done)
	{
		return;
	}

	// Initialize Internal File System
	InternalFS.begin();

	// Check if file exists
	lora_file.open(settings_name, FILE_O_READ);
	if (!lora_file)
	{
		API_LOG("FLASH", "File doesn't exist, force format");
		delay(1000);
		flash_reset();
		lora_file.open(settings_name, FILE_O_READ);
	}

	uint8_t markers[2] = {0};
	lora_file.read(markers, 2);
	if ((markers[0] == 0xAA) && (markers[1] == LORAWAN_COMPAT_MARKER))
	{
		API_LOG("FLASH", "File has old structure, merge into new structure");
		// Found old structure
		lora_file.close();
		// Read data into old structure
		s_loracompat_settings old_struct;
		lora_file.open(settings_name, FILE_O_READ);
		lora_file.read((uint8_t *)&old_struct, sizeof(s_loracompat_settings));
		lora_file.close();
		// Merge old structure into new structure
		g_lorawan_settings.adr_enabled = old_struct.adr_enabled;
		g_lorawan_settings.app_port = old_struct.app_port;
		g_lorawan_settings.auto_join = old_struct.auto_join;
		g_lorawan_settings.confirmed_msg_enabled = old_struct.confirmed_msg_enabled;
		g_lorawan_settings.data_rate = old_struct.data_rate;
		g_lorawan_settings.duty_cycle_enabled = old_struct.duty_cycle_enabled;
		g_lorawan_settings.join_trials = old_struct.join_trials;
		g_lorawan_settings.lora_class = old_struct.lora_class;
		g_lorawan_settings.lora_region = old_struct.lora_region;
		memcpy(g_lorawan_settings.node_app_eui, old_struct.node_app_eui, 8);
		memcpy(g_lorawan_settings.node_app_key, old_struct.node_app_key, 16);
		memcpy(g_lorawan_settings.node_apps_key, old_struct.node_apps_key, 16);
		g_lorawan_settings.node_dev_addr = old_struct.node_dev_addr;
		memcpy(g_lorawan_settings.node_device_eui, old_struct.node_device_eui, 8);
		memcpy(g_lorawan_settings.node_nws_key, old_struct.node_nws_key, 16);
		g_lorawan_settings.otaa_enabled = old_struct.otaa_enabled;
		g_lorawan_settings.public_network = old_struct.public_network;
		g_lorawan_settings.send_repeat_time = old_struct.send_repeat_time;
		g_lorawan_settings.subband_channels = old_struct.subband_channels;
		g_lorawan_settings.tx_power = old_struct.tx_power;
		save_settings();
		// delay(1000);
		// sd_nvic_SystemReset();
	}
	else
	{
		// Found new structure
		lora_file.close();
		lora_file.open(settings_name, FILE_O_READ);
		lora_file.read((uint8_t *)&g_lorawan_settings, sizeof(s_lorawan_settings));
		lora_file.close();
		// Check if it is LPWAN settings
		if ((g_lorawan_settings.valid_mark_1 != 0xAA) || (g_lorawan_settings.valid_mark_2 != LORAWAN_DATA_MARKER))
		{
			// Data is not valid, reset to defaults
			API_LOG("FLASH", "Invalid data set, deleting and restart node");
			InternalFS.format();
			delay(1000);
			sd_nvic_SystemReset();
		}
		log_settings();
		init_flash_done = true;
	}
}

/**
 * @brief Save changed settings if required
 * 
 * @return boolean 
 * 			result of saving
 */
boolean save_int_settings(void)
{
	bool result = true;
	// Read saved content
	lora_file.open(settings_name, FILE_O_READ);
	if (!lora_file)
	{
		API_LOG("FLASH", "File doesn't exist, force format");
		delay(100);
		flash_reset();
		lora_file.open(settings_name, FILE_O_READ);
	}
	lora_file.read((uint8_t *)&g_flash_content, sizeof(s_lorawan_settings));
	lora_file.close();
	if (memcmp((void *)&g_flash_content, (void *)&g_lorawan_settings, sizeof(s_lorawan_settings)) != 0)
	{
		API_LOG("FLASH", "Flash content changed, writing new data");
		delay(100);

		InternalFS.remove(settings_name);

		if (lora_file.open(settings_name, FILE_O_WRITE))
		{
			lora_file.write((uint8_t *)&g_lorawan_settings, sizeof(s_lorawan_settings));
			lora_file.flush();
		}
		else
		{
			result = false;
		}
		lora_file.close();
	}
	log_settings();
	return result;
}

/**
 * @brief Reset content of the filesystem
 * 
 */
void flash_int_reset(void)
{
	InternalFS.format();
	if (lora_file.open(settings_name, FILE_O_WRITE))
	{
		s_lorawan_settings default_settings;
		lora_file.write((uint8_t *)&default_settings, sizeof(s_lorawan_settings));
		lora_file.flush();
		lora_file.close();
	}
}

/**
 * @brief Printout of all settings
 * 
 */
void ble_log_settings(void)
{
	g_ble_uart.printf("Saved settings:");
	delay(50);
	g_ble_uart.printf("Marks: %02X %02X", g_lorawan_settings.valid_mark_1, g_lorawan_settings.valid_mark_2);
	delay(50);
	g_ble_uart.printf("Dev EUI %02X%02X%02X%02X%02X%02X%02X%02X", g_lorawan_settings.node_device_eui[0], g_lorawan_settings.node_device_eui[1],
					  g_lorawan_settings.node_device_eui[2], g_lorawan_settings.node_device_eui[3],
					  g_lorawan_settings.node_device_eui[4], g_lorawan_settings.node_device_eui[5],
					  g_lorawan_settings.node_device_eui[6], g_lorawan_settings.node_device_eui[7]);
	delay(50);
	g_ble_uart.printf("App EUI %02X%02X%02X%02X%02X%02X%02X%02X", g_lorawan_settings.node_app_eui[0], g_lorawan_settings.node_app_eui[1],
					  g_lorawan_settings.node_app_eui[2], g_lorawan_settings.node_app_eui[3],
					  g_lorawan_settings.node_app_eui[4], g_lorawan_settings.node_app_eui[5],
					  g_lorawan_settings.node_app_eui[6], g_lorawan_settings.node_app_eui[7]);
	delay(50);
	g_ble_uart.printf("App Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
					  g_lorawan_settings.node_app_key[0], g_lorawan_settings.node_app_key[1],
					  g_lorawan_settings.node_app_key[2], g_lorawan_settings.node_app_key[3],
					  g_lorawan_settings.node_app_key[4], g_lorawan_settings.node_app_key[5],
					  g_lorawan_settings.node_app_key[6], g_lorawan_settings.node_app_key[7],
					  g_lorawan_settings.node_app_key[8], g_lorawan_settings.node_app_key[9],
					  g_lorawan_settings.node_app_key[10], g_lorawan_settings.node_app_key[11],
					  g_lorawan_settings.node_app_key[12], g_lorawan_settings.node_app_key[13],
					  g_lorawan_settings.node_app_key[14], g_lorawan_settings.node_app_key[15]);
	delay(50);
	g_ble_uart.printf("Dev Addr %08lX", g_lorawan_settings.node_dev_addr);
	delay(50);
	g_ble_uart.printf("NWS Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
					  g_lorawan_settings.node_nws_key[0], g_lorawan_settings.node_nws_key[1],
					  g_lorawan_settings.node_nws_key[2], g_lorawan_settings.node_nws_key[3],
					  g_lorawan_settings.node_nws_key[4], g_lorawan_settings.node_nws_key[5],
					  g_lorawan_settings.node_nws_key[6], g_lorawan_settings.node_nws_key[7],
					  g_lorawan_settings.node_nws_key[8], g_lorawan_settings.node_nws_key[9],
					  g_lorawan_settings.node_nws_key[10], g_lorawan_settings.node_nws_key[11],
					  g_lorawan_settings.node_nws_key[12], g_lorawan_settings.node_nws_key[13],
					  g_lorawan_settings.node_nws_key[14], g_lorawan_settings.node_nws_key[15]);
	delay(50);
	g_ble_uart.printf("Apps Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
					  g_lorawan_settings.node_apps_key[0], g_lorawan_settings.node_apps_key[1],
					  g_lorawan_settings.node_apps_key[2], g_lorawan_settings.node_apps_key[3],
					  g_lorawan_settings.node_apps_key[4], g_lorawan_settings.node_apps_key[5],
					  g_lorawan_settings.node_apps_key[6], g_lorawan_settings.node_apps_key[7],
					  g_lorawan_settings.node_apps_key[8], g_lorawan_settings.node_apps_key[9],
					  g_lorawan_settings.node_apps_key[10], g_lorawan_settings.node_apps_key[11],
					  g_lorawan_settings.node_apps_key[12], g_lorawan_settings.node_apps_key[13],
					  g_lorawan_settings.node_apps_key[14], g_lorawan_settings.node_apps_key[15]);
	delay(50);
	g_ble_uart.printf("OTAA %s", g_lorawan_settings.otaa_enabled ? "enabled" : "disabled");
	delay(50);
	g_ble_uart.printf("ADR %s", g_lorawan_settings.adr_enabled ? "enabled" : "disabled");
	delay(50);
	g_ble_uart.printf("%s Network", g_lorawan_settings.public_network ? "Public" : "Private");
	delay(50);
	g_ble_uart.printf("Dutycycle %s", g_lorawan_settings.duty_cycle_enabled ? "enabled" : "disabled");
	delay(50);
	g_ble_uart.printf("Repeat time %ld", g_lorawan_settings.send_repeat_time);
	delay(50);
	g_ble_uart.printf("Join trials %d", g_lorawan_settings.join_trials);
	delay(50);
	g_ble_uart.printf("TX Power %d", g_lorawan_settings.tx_power);
	delay(50);
	g_ble_uart.printf("DR %d", g_lorawan_settings.data_rate);
	delay(50);
	g_ble_uart.printf("Class %d", g_lorawan_settings.lora_class);
	delay(50);
	g_ble_uart.printf("Subband %d", g_lorawan_settings.subband_channels);
	delay(50);
	g_ble_uart.printf("Auto join %s", g_lorawan_settings.auto_join ? "enabled" : "disabled");
	delay(50);
	g_ble_uart.printf("Fport %d", g_lorawan_settings.app_port);
	delay(50);
	g_ble_uart.printf("%s Message", g_lorawan_settings.confirmed_msg_enabled ? "Confirmed" : "Unconfirmed");
	delay(50);
	g_ble_uart.printf("Region %s", region_names[g_lorawan_settings.lora_region]);
	delay(50);
	g_ble_uart.printf("Mode %s", g_lorawan_settings.lorawan_enable ? "LPWAN" : "P2P");
	delay(50);
	g_ble_uart.printf("P2P frequency %ld", g_lorawan_settings.p2p_frequency);
	delay(50);
	g_ble_uart.printf("P2P TX Power %d", g_lorawan_settings.p2p_tx_power);
	delay(50);
	g_ble_uart.printf("P2P BW %d", g_lorawan_settings.p2p_bandwidth);
	delay(50);
	g_ble_uart.printf("P2P SF %d", g_lorawan_settings.p2p_sf);
	delay(50);
	g_ble_uart.printf("P2P CR %d", g_lorawan_settings.p2p_cr);
	delay(50);
	g_ble_uart.printf("P2P Preamble length %d", g_lorawan_settings.p2p_preamble_len);
	delay(50);
	g_ble_uart.printf("P2P Symbol Timeout %d", g_lorawan_settings.p2p_symbol_timeout);
	delay(50);
}

#endif