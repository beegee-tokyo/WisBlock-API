/**
 * @file flash-rak15001.cpp
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Initialize, read and write parameters from/to internal flash memory
 * @version 0.1
 * @date 2022-04-17
 *
 * @copyright Copyright (c) 2022
 *
 */
#include "WisBlock-API.h"
#include <RAK_FLASH_SPI.h>

// SPI Flash Interface instance
#ifdef ARDUINO_ARCH_RP2040
MbedSPI SPI_FLASH(PIN_SPI_MISO, PIN_SPI_MOSI, PIN_SPI_SCK);
RAK_FlashInterface_SPI g_flashTransport(SS, SPI_FLASH);
#else
RAK_FlashInterface_SPI g_flashTransport(SS, SPI);
#endif

/** SPI Flash instance */
RAK_FLASH_SPI g_flash(&g_flashTransport);

/** Flash definition structure for GD25Q16C Flash */
SPIFlash_Device_t g_RAK15001{
	.total_size = (1UL << 21),
	.start_up_time_us = 5000,
	.manufacturer_id = 0xc8,
	.memory_type = 0x40,
	.capacity = 0x15,
	.max_clock_speed_mhz = 15,
	.quad_enable_bit_mask = 0x00,
	.has_sector_protection = false,
	.supports_fast_read = true,
	.supports_qspi = false,
	.supports_qspi_writes = false,
	.write_status_register_split = false,
	.single_status_byte = true,
};

/** Flag if RAK15001 was found */
bool g_rak15001_flash = false;
/** Flag if external NVRAM was found */
bool g_ext_nvram = false;

// Forward declaration
// bool wait_with_timeout(void);
bool init_rak15001_flash(void);
bool init_int_flash(void);
boolean save_rak15001_settings(void);
boolean save_int_settings(void);
void flash_rak15001_reset(void);
void flash_int_reset(void);
void flash_reset(void);
bool read_rak15001(uint16_t sector, uint8_t *buffer, uint16_t size);
bool write_rak15001(uint16_t sector, uint8_t *buffer, uint16_t size);

bool init_flash_done = false;

/**
 * @brief Initialize internal or external NVRAM for the settings
 *
 */
void init_flash(void)
{
	if (init_flash_done)
	{
		return;
	}

	if (init_rak15001_flash())
	{
		g_rak15001_flash = true;
		g_ext_nvram = true;
	}
	/// \todo Add FRAM support here
	else
	{
		init_int_flash();
	}
	return;
}

/**
 * @brief Save settings to internal or external NVRAM
 *
 * @return true save success
 * @return false save failed
 */
bool save_settings(void)
{
	if (g_ext_nvram)
	{
		if (g_rak15001_flash)
		{
			return save_rak15001_settings();
		}
	}

	// Using internal Flash memory
	return save_int_settings();
}

/**
 * @brief Reset settings in internal or external NVRAM
 *
 */
void flash_reset(void)
{
	if (g_ext_nvram)
	{
		if (g_rak15001_flash)
		{
			return flash_rak15001_reset();
		}
	}

	// Using internal Flash memory
	flash_int_reset();
}

/**
 * @brief Initialize access to external NVRAM
 *
 */
bool init_rak15001_flash(void)
{
	if (!g_flash.begin(&g_RAK15001)) // Start access to the flash
	{
		API_LOG("FLASH", "Flash access failed, check the settings");
		return false;
	}
	if (!g_flash.waitUntilReady(5000))
	{
		API_LOG("FLASH", "Busy timeout");
		return false;
	}

	API_LOG("FLASH", "Device ID: 0x%02X", g_flash.getJEDECID());
	API_LOG("FLASH", "Size: %ld", g_flash.size());
	API_LOG("FLASH", "Pages: %d", g_flash.numPages());
	API_LOG("FLASH", "Page Size: %d", g_flash.pageSize());

	// Check if settings exists
	uint8_t markers[2] = {0};
	if (!read_rak15001(0, markers, 2))
	{
		API_LOG("FLASH", "Read failed");
		return false;
	}

	// Check if it is LPWAN settings
	if ((markers[0] != 0xAA) || (markers[1] != LORAWAN_DATA_MARKER))
	{
		// Data is not valid, reset to defaults
		API_LOG("FLASH", "Invalid data set, deleting and restart node");
		flash_rak15001_reset();
	}

	if (!read_rak15001(0, (uint8_t *)&g_lorawan_settings, sizeof(s_lorawan_settings)))
	{
		API_LOG("FLASH", "Read failed");
		return false;
	}

	init_flash_done = true;
	return true;
}

/**
 * @brief Save changed settings if required
 *
 * @return boolean
 * 			result of saving
 */
boolean save_rak15001_settings(void)
{
	s_lorawan_settings g_flash_content;
	if (!read_rak15001(0, (uint8_t *)&g_flash_content, sizeof(s_lorawan_settings)))
	{
		API_LOG("FLASH", "Save - Read back failed");
		return false;
	}

	if (memcmp((void *)&g_flash_content, (void *)&g_lorawan_settings, sizeof(s_lorawan_settings)) != 0)
	{
		API_LOG("FLASH", "Flash content changed, writing new data");
		delay(100);

		if (!write_rak15001(0, (uint8_t *)&g_lorawan_settings, sizeof(s_lorawan_settings)))
		{
			API_LOG("FLASH", "Save failed");
			return false;
		}

		log_settings();
		return true;
	}
	return true;
}

/**
 * @brief Reset content of the filesystem
 *
 */
void flash_rak15001_reset(void)
{
	s_lorawan_settings default_settings;
	if (!write_rak15001(0, (uint8_t *)&default_settings, sizeof(s_lorawan_settings)))
	{
		API_LOG("FLASH", "Reset failed");
	}
}

/**
 * @brief Read a block of bytes from the flash
 *
 * @param sector Sector in Flash (0 to 511)
 * @param buffer Buffer to write data into
 * @param size Number of bytes
 * @return true if success
 * @return false if failed
 */
bool read_rak15001(uint16_t sector, uint8_t *buffer, uint16_t size)
{
	if (sector > 511)
	{
		API_LOG("FLASH", "Invalid sector");
		return false;
	}

	// Read the bytes
	if (!g_flash.readBuffer(sector * 4096, buffer, size))
	{
		API_LOG("FLASH", "Read failed");
		return false;
	}
	return true;
}

/**
 * @brief Write a block of bytes into the flash
 *
 * @param sector Sector in Flash (0 to 511)
 * @param buffer Buffer with the data to write
 * @param size Number of bytes
 * @return true if success
 * @return false if failed
 */
bool write_rak15001(uint16_t sector, uint8_t *buffer, uint16_t size)
{
	if (sector > 511)
	{
		API_LOG("FLASH", "Invalid sector");
		return false;
	}

	// Format the sector
	if (!g_flash.eraseSector(sector))
	{
		API_LOG("FLASH", "Erase failed");
	}

	uint8_t check_buff[size];
	// Write the bytes
	if (!g_flash.writeBuffer(sector * 4096, buffer, size))
	{
		API_LOG("FLASH", "Write failed");
		return false;
	}

	if (!g_flash.waitUntilReady(5000))
	{
		API_LOG("FLASH", "Busy timeout");
		return false;
	}

	// Read back the data
	if (!g_flash.readBuffer(sector * 4096, check_buff, size))
	{
		API_LOG("FLASH", "Read back failed");
		return false;
	}

	// Check if read data is same as requested data
	if (memcmp(check_buff, buffer, size) != 0)
	{
		API_LOG("FLASH", "Bytes read back are not the same as written");
		Serial.println("Adr  Write buffer     Read back");
		for (int idx = 0; idx < size; idx++)
		{
			Serial.printf("%03d  %02X   %02X\r\n", idx, buffer[idx], check_buff[idx]);
		}
		Serial.println(" ");
		return false;
	}
	return true;
}

// /**
//  * @brief Wait for Flash Busy end with 5 seconds timeout
//  *
//  * @return true if Busy end
//  * @return false if timeout occured
//  */
// bool wait_with_timeout(void)
// {
// 	time_t start_time = millis();
// 	while ((millis() - start_time) < 2000)
// 	{
// 		if (!(g_flash.readStatus() & 0x03))
// 		{
// 			return true;
// 		}
// 	}
// 	return false;
// }

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
