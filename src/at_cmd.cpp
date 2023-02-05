/**
 * @file at_cmd.cpp
 * @author Taylor Lee (taylor.lee@rakwireless.com)
 * @brief AT command parser
 * @version 0.1
 * @date 2021-04-27
 *
 * @copyright Copyright (c) 2021
 *
 */
#include "at_cmd.h"

static char atcmd[ATCMD_SIZE];
static char cmd_result[ATCMD_SIZE];
static uint16_t atcmd_index = 0;
char g_at_query_buf[ATQUERY_SIZE];

bool has_custom_at = false;

/** LoRaWAN application data buffer. */
uint8_t m_lora_app_data_buffer[256];

/** Bandwidths as char arrays */
char *bandwidths[] = {(char *)"125", (char *)"250", (char *)"500", (char *)"062", (char *)"041", (char *)"031", (char *)"020", (char *)"015", (char *)"010", (char *)"007"};

/** Regions as char arrays */
char *region_names[] = {(char *)"AS923", (char *)"AU915", (char *)"CN470", (char *)"CN779",
						(char *)"EU433", (char *)"EU868", (char *)"KR920", (char *)"IN865",
						(char *)"US915", (char *)"AS923-2", (char *)"AS923-3", (char *)"AS923-4", (char *)"RU864"};

/** Region map between WisToolBox and API*/
uint8_t wtb_regions[] = {8, 6, 1, 12, 0, 4, 7, 3, 5, 9, 10, 11, 2};

/** Region map between API and WisToolBox*/
uint8_t api_regions[] = {4, 2, 12, 7, 5, 8, 1, 6, 0, 9, 10, 11, 3};

/**
 * @brief Set the device to new settings
 * 
 */
void set_new_config(void)
{
	Radio.Sleep();
	Radio.SetTxConfig(MODEM_LORA, g_lorawan_settings.p2p_tx_power, 0, g_lorawan_settings.p2p_bandwidth,
					  g_lorawan_settings.p2p_sf, g_lorawan_settings.p2p_cr,
					  g_lorawan_settings.p2p_preamble_len, false,
					  true, 0, 0, false, 5000);

	Radio.SetRxConfig(MODEM_LORA, g_lorawan_settings.p2p_bandwidth, g_lorawan_settings.p2p_sf,
					  g_lorawan_settings.p2p_cr, 0, g_lorawan_settings.p2p_preamble_len,
					  g_lorawan_settings.p2p_symbol_timeout, false,
					  0, true, 0, 0, false, true);
	Radio.Rx(0);
}

/**
 * @brief Convert Hex string into uint8_t array
 *
 * @param hex Hex string
 * @param bin uint8_t
 * @param bin_length Length of array
 * @return int -1 if conversion failed
 */
static int hex2bin(const char *hex, uint8_t *bin, uint16_t bin_length)
{
	uint16_t hex_length = strlen(hex);
	const char *hex_end = hex + hex_length;
	uint8_t *cur = bin;
	uint8_t num_chars = hex_length & 1;
	uint8_t byte = 0;

	if (hex_length % 2 != 0)
	{
		return -1;
	}

	if (hex_length / 2 > bin_length)
	{
		return -1;
	}

	while (hex < hex_end)
	{
		if ('A' <= *hex && *hex <= 'F')
		{
			byte |= 10 + (*hex - 'A');
		}
		else if ('a' <= *hex && *hex <= 'f')
		{
			byte |= 10 + (*hex - 'a');
		}
		else if ('0' <= *hex && *hex <= '9')
		{
			byte |= *hex - '0';
		}
		else
		{
			return -1;
		}
		hex++;
		num_chars++;

		if (num_chars >= 2)
		{
			num_chars = 0;
			*cur++ = byte;
			byte = 0;
		}
		else
		{
			byte <<= 4;
		}
	}
	return cur - bin;
}

/**
 * @brief Print out all parameters over UART and BLE
 *
 */
void at_settings(void)
{
	AT_PRINTF("Device status:");
#ifdef NRF52_SERIES
	AT_PRINTF("   RAK4631");
#endif
#ifdef ARDUINO_ARCH_RP2040
	AT_PRINTF("   RAK11310");
#endif
#ifdef ESP32
	AT_PRINTF("   RAK11200");
#endif
	AT_PRINTF("   Auto join %s", g_lorawan_settings.auto_join ? "enabled" : "disabled");
	AT_PRINTF("   Mode %s", g_lorawan_settings.lorawan_enable ? "LPWAN" : "P2P");
	AT_PRINTF("   Network %s", g_lpwan_has_joined ? "joined" : "not joined");
	AT_PRINTF("   Send Frequency %ld", g_lorawan_settings.send_repeat_time / 1000);
	AT_PRINTF("LPWAN status:");
	AT_PRINTF("   Dev EUI %02X%02X%02X%02X%02X%02X%02X%02X", g_lorawan_settings.node_device_eui[0], g_lorawan_settings.node_device_eui[1],
			  g_lorawan_settings.node_device_eui[2], g_lorawan_settings.node_device_eui[3],
			  g_lorawan_settings.node_device_eui[4], g_lorawan_settings.node_device_eui[5],
			  g_lorawan_settings.node_device_eui[6], g_lorawan_settings.node_device_eui[7]);
	AT_PRINTF("   App EUI %02X%02X%02X%02X%02X%02X%02X%02X", g_lorawan_settings.node_app_eui[0], g_lorawan_settings.node_app_eui[1],
			  g_lorawan_settings.node_app_eui[2], g_lorawan_settings.node_app_eui[3],
			  g_lorawan_settings.node_app_eui[4], g_lorawan_settings.node_app_eui[5],
			  g_lorawan_settings.node_app_eui[6], g_lorawan_settings.node_app_eui[7]);
	AT_PRINTF("   App Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			  g_lorawan_settings.node_app_key[0], g_lorawan_settings.node_app_key[1],
			  g_lorawan_settings.node_app_key[2], g_lorawan_settings.node_app_key[3],
			  g_lorawan_settings.node_app_key[4], g_lorawan_settings.node_app_key[5],
			  g_lorawan_settings.node_app_key[6], g_lorawan_settings.node_app_key[7],
			  g_lorawan_settings.node_app_key[8], g_lorawan_settings.node_app_key[9],
			  g_lorawan_settings.node_app_key[10], g_lorawan_settings.node_app_key[11],
			  g_lorawan_settings.node_app_key[12], g_lorawan_settings.node_app_key[13],
			  g_lorawan_settings.node_app_key[14], g_lorawan_settings.node_app_key[15]);
	AT_PRINTF("   Dev Addr %08lX", g_lorawan_settings.node_dev_addr);
	AT_PRINTF("   NWS Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			  g_lorawan_settings.node_nws_key[0], g_lorawan_settings.node_nws_key[1],
			  g_lorawan_settings.node_nws_key[2], g_lorawan_settings.node_nws_key[3],
			  g_lorawan_settings.node_nws_key[4], g_lorawan_settings.node_nws_key[5],
			  g_lorawan_settings.node_nws_key[6], g_lorawan_settings.node_nws_key[7],
			  g_lorawan_settings.node_nws_key[8], g_lorawan_settings.node_nws_key[9],
			  g_lorawan_settings.node_nws_key[10], g_lorawan_settings.node_nws_key[11],
			  g_lorawan_settings.node_nws_key[12], g_lorawan_settings.node_nws_key[13],
			  g_lorawan_settings.node_nws_key[14], g_lorawan_settings.node_nws_key[15]);
	AT_PRINTF("   Apps Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
			  g_lorawan_settings.node_apps_key[0], g_lorawan_settings.node_apps_key[1],
			  g_lorawan_settings.node_apps_key[2], g_lorawan_settings.node_apps_key[3],
			  g_lorawan_settings.node_apps_key[4], g_lorawan_settings.node_apps_key[5],
			  g_lorawan_settings.node_apps_key[6], g_lorawan_settings.node_apps_key[7],
			  g_lorawan_settings.node_apps_key[8], g_lorawan_settings.node_apps_key[9],
			  g_lorawan_settings.node_apps_key[10], g_lorawan_settings.node_apps_key[11],
			  g_lorawan_settings.node_apps_key[12], g_lorawan_settings.node_apps_key[13],
			  g_lorawan_settings.node_apps_key[14], g_lorawan_settings.node_apps_key[15]);
	AT_PRINTF("   OTAA %s", g_lorawan_settings.otaa_enabled ? "enabled" : "disabled");
	AT_PRINTF("   ADR %s", g_lorawan_settings.adr_enabled ? "enabled" : "disabled");
	AT_PRINTF("   %s Network", g_lorawan_settings.public_network ? "Public" : "Private");
	AT_PRINTF("   Dutycycle %s", g_lorawan_settings.duty_cycle_enabled ? "enabled" : "disabled");
	AT_PRINTF("   Join trials %d", g_lorawan_settings.join_trials);
	AT_PRINTF("   TX Power %d", g_lorawan_settings.tx_power);
	AT_PRINTF("   DR %d", g_lorawan_settings.data_rate);
	AT_PRINTF("   Class %d", g_lorawan_settings.lora_class);
	AT_PRINTF("   Subband %d", g_lorawan_settings.subband_channels);
	AT_PRINTF("   Fport %d", g_lorawan_settings.app_port);
	AT_PRINTF("   %s Message", g_lorawan_settings.confirmed_msg_enabled ? "Confirmed" : "Unconfirmed");
	AT_PRINTF("   Region %s", region_names[g_lorawan_settings.lora_region]);
	AT_PRINTF("LoRa P2P status:");
	AT_PRINTF("   P2P frequency %ld", g_lorawan_settings.p2p_frequency);
	AT_PRINTF("   P2P TX Power %d", g_lorawan_settings.p2p_tx_power);
	AT_PRINTF("   P2P BW %s", bandwidths[g_lorawan_settings.p2p_bandwidth]);
	AT_PRINTF("   P2P SF %d", g_lorawan_settings.p2p_sf);
	AT_PRINTF("   P2P CR %d", g_lorawan_settings.p2p_cr);
	AT_PRINTF("   P2P Preamble length %d", g_lorawan_settings.p2p_preamble_len);
	AT_PRINTF("   P2P Symbol Timeout %d", g_lorawan_settings.p2p_symbol_timeout);
}

/**
 * @brief Query LoRa mode 
 * 
 * @return int AT_SUCCESS
 */
static int at_query_mode(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.lorawan_enable ? 1 : 0);
	return AT_SUCCESS;
}

/**
 * @brief Switch LoRa mode
 *
 * @param str char array with mode
 *			'0' = LoRa P2P
 *			'1' = LoRaWAN
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_PARA_VAL
 */
static int at_exec_mode(char *str)
{
	bool need_restart = false;
	if (str[0] == '0')
	{
		if (g_lorawan_settings.lorawan_enable)
		{
			need_restart = true;
		}
		g_lorawan_settings.lorawan_enable = false;
	}
	else if (str[0] == '1')
	{
		if (!g_lorawan_settings.lorawan_enable)
		{
			need_restart = true;
		}
		g_lorawan_settings.lorawan_enable = true;
	}
	else
	{
		return AT_ERRNO_PARA_VAL;
	}

	save_settings();

	if (need_restart)
	{
		delay(100);
		api_reset();
	}
	return AT_SUCCESS;
}

/**
 * @brief Get current LoRa P2P frequency
 * 
 * @return int AT_SUCCESS
 */
static int at_query_p2p_freq(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", g_lorawan_settings.p2p_frequency);
	return AT_SUCCESS;
}

/**
 * @brief Set LoRa P2P frequency
 *
 * @param str frequency as char array, valid values are '525000000' to '960000000'
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_p2p_freq(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	long freq = strtol(str, NULL, 0);

	if ((freq < 525000000) || (freq > 960000000))
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.p2p_frequency = freq;
	save_settings();

	set_new_config();
	return AT_SUCCESS;
}

/**
 * @brief Get LoRa P2P spreading factor
 * 
 * @return int AT_SUCCESS
 */
static int at_query_p2p_sf(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.p2p_sf);
	return AT_SUCCESS;
}

/**
 * @brief Set LoRa P2P spreading factor
 *
 * @param str spreading factor as char array, valid values '7' to '12'
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_p2p_sf(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	long sf = strtol(str, NULL, 0);

	if ((sf < 7) || (sf > 12))
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.p2p_sf = sf;
	save_settings();

	set_new_config();
	return AT_SUCCESS;
}

/**
 * @brief Get current LoRa P2P bandwidth
 * 
 * @return int AT_SUCCESS
 */
static int at_query_p2p_bw(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%s", bandwidths[g_lorawan_settings.p2p_bandwidth]);
	return AT_SUCCESS;
}

/**
 * @brief Set LoRa P2P bandwidth
 *
 * @param str bandwidth as char array, valid values are '0' = 125, '1' = 250, '2' = 500, '3' = 7.8, '4' = 10.4, '5' = 15.63, '6' = 20.83, '7' = 31.25, '8' = 41.67, '9' = 62.5
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_p2p_bw(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	for (int idx = 0; idx < 10; idx++)
	{
		if (strcmp(str, bandwidths[idx]) == 0)
		{
			g_lorawan_settings.p2p_bandwidth = idx;
			save_settings();

			set_new_config();
			return AT_SUCCESS;
		}
	}
	return AT_ERRNO_PARA_VAL;
}

/**
 * @brief Get current LoRa P2P coding rate
 * 
 * @return int AT_SUCCESS
 */
static int at_query_p2p_cr(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.p2p_cr);
	return AT_SUCCESS;
}

/**
 * @brief Set LoRa P2P coding rate
 *
 * @param str coding rate as char array valid values are '1' = 4/5, '2' = '4/6, '3' = 4/7, '4' = 4/8
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_p2p_cr(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	long cr = strtol(str, NULL, 0);

	if ((cr < 1) || (cr > 4))
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.p2p_cr = cr;
	save_settings();

	set_new_config();
	return AT_SUCCESS;
}

/**
 * @brief Get current LoRa P2P preamble length
 * 
 * @return int AT_SUCCESS
 */
static int at_query_p2p_pl(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.p2p_preamble_len);
	return AT_SUCCESS;
}

/**
 * @brief Set LoRa P2P preamble length
 *
 * @param str preamble length as char array, valid values are '0' to '255'
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_p2p_pl(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	long preamble_len = strtol(str, NULL, 0);

	if ((preamble_len < 0) || (preamble_len > 256))
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.p2p_preamble_len = preamble_len;
	save_settings();

	set_new_config();
	return AT_SUCCESS;
}

/**
 * @brief Get current LoRa P2P TX power
 * 
 * @return int AT_SUCCESS
 */
static int at_query_p2p_txp(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.p2p_tx_power);
	return AT_SUCCESS;
}

/**
 * @brief Set LoRa P2P TX power
 *
 * @param str TX power as char array, valid values = '1' to '22'
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_p2p_txp(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	long txp = strtol(str, NULL, 0);

	if ((txp < 0) || (txp > 23))
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.p2p_tx_power = txp;
	save_settings();

	set_new_config();
	return AT_SUCCESS;
}

/**
 * @brief Get current LoRa P2P settings
 * 
 * @return int AT_SUCCESS
 */
static int at_query_p2p_config(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld:%d:%s:%d:%d:%d",
			 g_lorawan_settings.p2p_frequency,
			 g_lorawan_settings.p2p_sf,
			 bandwidths[g_lorawan_settings.p2p_bandwidth],
			 g_lorawan_settings.p2p_cr,
			 g_lorawan_settings.p2p_preamble_len,
			 g_lorawan_settings.p2p_tx_power);
	return AT_SUCCESS;
}

/**
 * @brief Set LoRa P2P configuration
 *
 * @param str configuration as char array
 * 			format <frequency>:<spreading factor>:<bandwidth>:<coding rate>:<preamble length>:<TX power>
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL, AT_ERRNO_PARA_NUM
 */
static int at_exec_p2p_config(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	char *param;

	param = strtok(str, ":");

	if (param != NULL)
	{
		/* check frequency */
		long freq = strtol(param, NULL, 0);

		if ((freq < 525000000) || (freq > 960000000))
		{
			return AT_ERRNO_PARA_VAL;
		}

		g_lorawan_settings.p2p_frequency = freq;

		/* check SF */
		param = strtok(NULL, ":");
		if (param != NULL)
		{
			long sf = strtol(param, NULL, 0);

			if ((sf < 7) || (sf > 12))
			{
				return AT_ERRNO_PARA_VAL;
			}

			g_lorawan_settings.p2p_sf = sf;

			// Check Bandwidth
			param = strtok(NULL, ":");
			if (param != NULL)
			{
				bool found_bw = false;
				for (int idx = 0; idx < 10; idx++)
				{
					if (strcmp(param, bandwidths[idx]) == 0)
					{
						g_lorawan_settings.p2p_bandwidth = idx;
						found_bw = true;
					}
				}
				if (!found_bw)
				{
					return AT_ERRNO_PARA_VAL;
				}

				// Check CR
				param = strtok(NULL, ":");
				if (param != NULL)
				{
					long cr = strtol(param, NULL, 0);

					if ((cr < 0) || (cr > 3))
					{
						return AT_ERRNO_PARA_VAL;
					}

					g_lorawan_settings.p2p_cr = cr;

					// Check Preamble length
					param = strtok(NULL, ":");
					if (param != NULL)
					{
						long preamble_len = strtol(param, NULL, 0);

						if ((preamble_len < 0) || (preamble_len > 256))
						{
							return AT_ERRNO_PARA_VAL;
						}

						g_lorawan_settings.p2p_preamble_len = preamble_len;

						// Check TX power
						param = strtok(NULL, ":");
						if (param != NULL)
						{
							long txp = strtol(param, NULL, 0);

							if ((txp < 0) || (txp > 23))
							{
								return AT_ERRNO_PARA_VAL;
							}

							g_lorawan_settings.p2p_tx_power = txp;

							save_settings();

							set_new_config();
							return AT_SUCCESS;
						}
					}
				}
			}
		}
	}
	return AT_ERRNO_PARA_NUM;
}

/**
 * @brief Send data packet over LoRa P2P
 *
 * @param str data packet as char array with data in ASCII Hex
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_p2p_send(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}

	int data_size = strlen(str);
	if (!(data_size % 2 == 0) || (data_size > 254))
	{
		return AT_ERRNO_PARA_VAL;
	}

	int buff_idx = 0;
	char buff_parse[3];
	for (int idx = 0; idx <= data_size + 1; idx += 2)
	{
		buff_parse[0] = str[idx];
		buff_parse[1] = str[idx + 1];
		buff_parse[2] = 0;
		m_lora_app_data_buffer[buff_idx] = strtol(buff_parse, NULL, 16);
		buff_idx++;
	}
	send_p2p_packet(m_lora_app_data_buffer, data_size / 2);
	return AT_SUCCESS;
}

/**
 * @brief Set P2P RX mode
 * 0 => TX mode, RX disabled
 * 1 ... 65533 => Enable RX for xxxx milliseconds
 * 65534 => Enable continous RX (restarts after TX)
 * 65535 => Enable RX until a packet was received, no timeout
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL, AT_ERRNO_PARA_NUM
 */
static int at_exec_p2p_receive(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}

	char *param = strtok(str, ":");

	if (param != NULL)
	{
		/* check RX window size */
		uint32_t rx_time = strtol(param, NULL, 0);
		API_LOG("AT", "Received RX time %d", rx_time);

		if (rx_time == 0)
		{
			// TX only mode
			g_lora_p2p_rx_mode = RX_MODE_NONE;
			g_lora_p2p_rx_time = 0;
			// Put Radio into sleep mode (stops receiving)
			Radio.Sleep();
			API_LOG("AT", "Set RX_MODE_NONE");
		}
		else if (rx_time == 65534)
		{
			// RX continous
			g_lora_p2p_rx_mode = RX_MODE_RX;
			g_lora_p2p_rx_time = 0;
			// Put Radio into continous RX mode
			Radio.Rx(0);
			API_LOG("AT", "Set RX_MODE_RX");
		}
		else if (rx_time == 65535)
		{
			// RX until packet received
			g_lora_p2p_rx_mode = RX_MODE_RX_WAIT;
			g_lora_p2p_rx_time = 0;
			// Put Radio into continous RX mode
			Radio.Rx(0);
			API_LOG("AT", "Set RX_MODE_RX_WAIT");
		}
		else if (rx_time < 65534)
		{
			// RX for specific time
			g_lora_p2p_rx_mode = RX_MODE_RX_TIMED;
			g_lora_p2p_rx_time = rx_time;
			// Put Radio into continous RX mode
			Radio.Rx(rx_time);
			API_LOG("AT", "Set RX_MODE_RX_TIMED");
		}
		else
		{
			return AT_ERRNO_PARA_VAL;
		}
		return AT_SUCCESS;
	}
	return AT_ERRNO_PARA_NUM;
}

/**
 * @brief Get current LoRa P2P receive mode
 * 
 * @return int AT_SUCCESS
 */
static int at_query_p2p_receive(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", g_lora_p2p_rx_time);
	return AT_SUCCESS;
}

/**
 * @brief AT+BAND=? Get regional frequency band
 *
 * @return int AT_SUCCESS;
 */
static int at_query_region(void)
{
	// 0: AS923 1: AU915 2: CN470 3: CN779 4: EU433 5: EU868 6: KR920 7: IN865 8: US915 9: AS923-2 10: AS923-3 11: AS923-4 12: RU864
	// snprintf(g_at_query_buf, ATQUERY_SIZE, "%d %s", g_lorawan_settings.lora_region, region_names[g_lorawan_settings.lora_region]);
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", wtb_regions[g_lorawan_settings.lora_region]);

	return AT_SUCCESS;
}

/**
 * @brief AT+BAND=xx Set regional frequency band
 *  Values: 0 = EU433, 1 = CN470, 2 = RU864, 3 = IN865, 4 = EU868, 5 = US915, 6 = AU915, 7 = KR920, 8 = AS923-1, 9 = AS923-2, 10 = AS923-3, 11 = AS923-4, 12 = CN779
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_region(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	char *param;
	uint8_t region;

	param = strtok(str, ",");
	if (param != NULL)
	{
		region = strtol(param, NULL, 0);
		// 0 = EU433, 1 = CN470, 2 = RU864, 3 = IN865, 4 = EU868, 5 = US915, 6 = AU915, 7 = KR920, 8 = AS923-1, 9 = AS923-2, 10 = AS923-3, 11 = AS923-4, 12 = CN779
		if (region > 12)
		{
			return AT_ERRNO_PARA_VAL;
		}
		g_lorawan_settings.lora_region = api_regions[region];
		save_settings();
	}
	else
	{
		return AT_ERRNO_PARA_VAL;
	}

	return AT_SUCCESS;
}

/**
 * @brief AT+MASK=? Get channel mask
 *  Only available for regions 5 = US915, 6 = AU915, 1 = CN470
 * @return int AT_SUCCESS;
 */
static int at_query_mask(void)
{
	if ((g_lorawan_settings.lora_region == LORAMAC_REGION_US915) || (g_lorawan_settings.lora_region == LORAMAC_REGION_AU915) || (g_lorawan_settings.lora_region == LORAMAC_REGION_CN470))
	{
		snprintf(g_at_query_buf, ATQUERY_SIZE, "%04X", 1 << (g_lorawan_settings.subband_channels - 1));

		return AT_SUCCESS;
	}
	return AT_ERRNO_NOALLOW;
}

/**
 * @brief AT+MASK=xx Set channel mask
 *  Only available for regions 5 = US915, 6 = AU915, 1 = CN470
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL, AT_ERRNO_PARA_NUM
 */
static int at_exec_mask(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	char *param;
	uint8_t mask;

	param = strtok(str, ",");
	if (param != NULL)
	{
		mask = strtol(param, NULL, 0);

		uint8_t maxBand = 1;
		switch (g_lorawan_settings.lora_region)
		{
		case LORAMAC_REGION_AU915:
			maxBand = 9;
			break;
		case LORAMAC_REGION_CN470:
			maxBand = 12;
			break;
		case LORAMAC_REGION_US915:
			maxBand = 9;
			break;
		default:
			return AT_ERRNO_PARA_VAL;
		}
		switch (mask)
		{
		case 0x0001:
			mask = 1;
			break;
		case 0x0002:
			mask = 2;
			break;
		case 0x0004:
			mask = 3;
			break;
		case 0x0008:
			mask = 4;
			break;
		case 0x0010:
			mask = 5;
			break;
		case 0x0020:
			mask = 6;
			break;
		case 0x0040:
			mask = 7;
			break;
		case 0x0080:
			mask = 8;
			break;
		case 0x0100:
			mask = 9;
			break;
		case 0x0200:
			mask = 10;
			break;
		case 0x0400:
			mask = 11;
			break;
		case 0x0800:
			mask = 12;
			break;
		default:
			mask = 99;
			break;
		}
		if ((mask == 0) || (mask > maxBand))
		{
			return AT_ERRNO_PARA_VAL;
		}
		g_lorawan_settings.subband_channels = mask;
		save_settings();
	}
	else
	{
		return AT_ERRNO_PARA_NUM;
	}

	return AT_SUCCESS;
}

/**
 * @brief AT+NJM=? Return current join modus
 *
 * @return int AT_SUCCESS;
 */
static int at_query_joinmode(void)
{
	int mode;
	mode = g_lorawan_settings.otaa_enabled == true ? 1 : 0;

	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", mode);
	return AT_SUCCESS;
}

/**
 * @brief AT+NJM=x Set current join modus
 *
 * @param str 1 = OTAA 0 = ABP
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_joinmode(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	int mode = strtol(str, NULL, 0);

	if (mode != 0 && mode != 1)
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.otaa_enabled = (mode == 1 ? true : false);
	save_settings();

	return AT_SUCCESS;
}

/**
 * @brief AT+DEVEUI=? Get current Device EUI
 *
 * @return int AT_SUCCESS;
 */
static int at_query_deveui(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE,
			 "%02X%02X%02X%02X%02X%02X%02X%02X",
			 g_lorawan_settings.node_device_eui[0],
			 g_lorawan_settings.node_device_eui[1],
			 g_lorawan_settings.node_device_eui[2],
			 g_lorawan_settings.node_device_eui[3],
			 g_lorawan_settings.node_device_eui[4],
			 g_lorawan_settings.node_device_eui[5],
			 g_lorawan_settings.node_device_eui[6],
			 g_lorawan_settings.node_device_eui[7]);
	return AT_SUCCESS;
}

/**
 * @brief AT+DEVEUI=<XXXXXXXXXXXXXXXX> Set current Device EUI
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_deveui(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	uint8_t len;
	uint8_t buf[8];

	len = hex2bin(str, buf, 8);

	if (len != 8)
	{
		return AT_ERRNO_PARA_VAL;
	}

	memcpy(g_lorawan_settings.node_device_eui, buf, 8);
	save_settings();

	return AT_SUCCESS;
}

/**
 * @brief AT+APPEUI=? Get current Application (Join) EUI
 *
 * @return int AT_SUCCESS;
 */
static int at_query_appeui(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE,
			 "%02X%02X%02X%02X%02X%02X%02X%02X",
			 g_lorawan_settings.node_app_eui[0],
			 g_lorawan_settings.node_app_eui[1],
			 g_lorawan_settings.node_app_eui[2],
			 g_lorawan_settings.node_app_eui[3],
			 g_lorawan_settings.node_app_eui[4],
			 g_lorawan_settings.node_app_eui[5],
			 g_lorawan_settings.node_app_eui[6],
			 g_lorawan_settings.node_app_eui[7]);
	return AT_SUCCESS;
}

/**
 * @brief AT+APPEUI=<XXXXXXXXXXXXXXXX> Set current Application (Join) EUI
 *
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_appeui(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	uint8_t len;
	uint8_t buf[8];

	len = hex2bin(str, buf, 8);
	if (len != 8)
	{
		return AT_ERRNO_PARA_VAL;
	}

	memcpy(g_lorawan_settings.node_app_eui, buf, 8);
	save_settings();

	return AT_SUCCESS;
}

/**
 * @brief AT+APPKEY=? Get current Application Key
 *
 * @return int AT_SUCCESS;
 */
static int at_query_appkey(void)
{
	uint8_t i;
	uint8_t len = 0;

	for (i = 0; i < 16; i++)
	{
		len += snprintf(g_at_query_buf + len, ATQUERY_SIZE - len, "%02X", g_lorawan_settings.node_app_key[i]);
		if (ATQUERY_SIZE <= len)
		{
			return -1;
		}
	}
	return AT_SUCCESS;
}

/**
 * @brief AT+APPKEY=<XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX> Set current Application (Join) EUI
 *
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_appkey(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	uint8_t buf[16];
	uint8_t len;

	len = hex2bin(str, buf, 16);
	if (len != 16)
	{
		return AT_ERRNO_PARA_VAL;
	}

	memcpy(g_lorawan_settings.node_app_key, buf, 16);
	save_settings();

	return AT_SUCCESS;
}

/**
 * @brief AT+DEVADDR=? Get device address
 *
 * @return int AT_SUCCESS;
 */
static int at_query_devaddr(void)
{
	if (otaaDevAddr != 0)
	{
		snprintf(g_at_query_buf, ATQUERY_SIZE, "%08lX", otaaDevAddr);
	}
	else
	{
		snprintf(g_at_query_buf, ATQUERY_SIZE, "%08lX", g_lorawan_settings.node_dev_addr);
	}
	return AT_SUCCESS;
}

/**
 * @brief AT+DEVADDR=<XXXXXXXX> Set device address
 *
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_devaddr(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	int i;
	uint8_t len;
	uint8_t buf[4];
	uint8_t swap_buf[4];

	len = hex2bin(str, buf, 4);
	if (len != 4)
	{
		return AT_ERRNO_PARA_VAL;
	}

	for (i = 0; i < 4; i++)
	{
		swap_buf[i] = buf[3 - i];
	}

	memcpy(&g_lorawan_settings.node_dev_addr, swap_buf, 4);
	save_settings();

	return AT_SUCCESS;
}

/**
 * @brief AT+APPSKEY=? Get application session key
 *
 * @return int AT_SUCCESS;
 */
static int at_query_appskey(void)
{
	uint8_t i;
	uint8_t len = 0;

	for (i = 0; i < 16; i++)
	{
		len += snprintf(g_at_query_buf + len, ATQUERY_SIZE - len, "%02X", g_lorawan_settings.node_apps_key[i]);
		if (ATQUERY_SIZE <= len)
		{
			return -1;
		}
	}

	return AT_SUCCESS;
}

/**
 * @brief AT+APPSKEY=<XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX> Set application session key
 *
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_appskey(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	uint8_t len;
	uint8_t buf[16];

	len = hex2bin(str, buf, 16);
	if (len != 16)
	{
		return AT_ERRNO_PARA_VAL;
	}

	memcpy(g_lorawan_settings.node_apps_key, buf, 16);
	save_settings();

	return AT_SUCCESS;
}

/**
 * @brief AT+NWSKEY=? Get network session key
 *
 * @return int AT_SUCCESS;
 */
static int at_query_nwkskey(void)
{
	uint8_t i;
	uint8_t len = 0;

	for (i = 0; i < 16; i++)
	{
		len += snprintf(g_at_query_buf + len, ATQUERY_SIZE - len, "%02X", g_lorawan_settings.node_nws_key[i]);
		if (ATQUERY_SIZE <= len)
		{
			return -1;
		}
	}

	return AT_SUCCESS;
}

/**
 * @brief AT+NWSKEY=<XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX> Set network session key
 *
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_nwkskey(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	uint8_t len;
	uint8_t buf[16];

	len = hex2bin(str, buf, 16);
	if (len != 16)
	{
		return AT_ERRNO_PARA_VAL;
	}

	memcpy(g_lorawan_settings.node_nws_key, buf, 16);
	save_settings();

	return AT_SUCCESS;
}

/**
 * @brief AT+CLASS=? Get device class
 *
 * @return int AT_SUCCESS;
 */
static int at_query_class(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%c", g_lorawan_settings.lora_class + 65);

	return AT_SUCCESS;
}

/**
 * @brief AT+CLASS=X Set device class
 *
 * @param str Class A or C, B not supported
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_class(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	uint8_t cls;
	char *param;

	param = strtok(str, ",");
	cls = (uint8_t)param[0];
	// Class B is not supported
	// if (cls != 'A' && cls != 'B' && cls != 'C')
	if (cls != 'A' && cls != 'C')
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.lora_class = cls - 65;
	save_settings();

	return AT_SUCCESS;
}

/**
 * @brief AT+NJM=? Get join mode
 *
 * @return int AT_SUCCESS;
 */
static int at_query_join(void)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	// Param1 = Join command: 1 for joining the network , 0 for stop joining
	// Param2 = Auto-Join config: 1 for Auto-join on power up) , 0 for no auto-join.
	// Param3 = Reattempt interval: 7 - 255 seconds
	// Param4 = No. of join attempts: 0 - 255
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d:%d:%d:%d", g_lpwan_has_joined ? 1 : 0, g_lorawan_settings.auto_join ? 1 : 0, 8, g_lorawan_settings.join_trials);

	return AT_SUCCESS;
}

/**
 * @brief AT+NJM=<Param1>,<Param2>,<Param3>,<Param4> Set join mode
 * Param1 = Join command: 1 for joining the network , 0 for stop joining (not supported)
 * Param2 = Auto-Join config: 1 for Auto-join on power up) , 0 for no auto-join.
 * Param3 = Reattempt interval: 7 - 255 seconds (ignored)
 * Param4 = No. of join attempts: 0 - 255
 *
 * @param str parameters as string
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL, AT_ERRNO_PARA_NUM
 */
static int at_exec_join(char *str)
{
	uint8_t bJoin;
	uint8_t autoJoin;
	uint8_t nbtrials;
	char *param;

	param = strtok(str, ":");

	/* check start or stop join parameter */
	bJoin = strtol(param, NULL, 0);
	if (bJoin != 1 && bJoin != 0)
	{
		return AT_ERRNO_PARA_VAL;
	}

	/* check auto join parameter */
	param = strtok(NULL, ":");
	if (param != NULL)
	{
		autoJoin = strtol(param, NULL, 0);
		if (autoJoin != 1 && autoJoin != 0)
		{
			return AT_ERRNO_PARA_VAL;
		}
		g_lorawan_settings.auto_join = (autoJoin == 1 ? true : false);

		if (!g_lorawan_settings.lorawan_enable)
		{
			if (bJoin == 0)
			{
				if (!g_lorawan_initialized)
				{
					init_lora();
				}
				if (!g_lorawan_settings.auto_join)
				{
					Radio.Sleep();
				}
			}

			if (bJoin == 1)
			{
				if (!g_lorawan_initialized)
				{
					init_lora();
				}
				else
				{
					Radio.Rx(0);
				}
			}

			save_settings();
			return AT_SUCCESS;
		}

		param = strtok(NULL, ":");
		if (param != NULL)
		{
			// join interval, not support yet.

			// join attemps number
			param = strtok(NULL, ":");
			if (param != NULL)
			{
				nbtrials = strtol(param, NULL, 0);
				if ((nbtrials == 0) && g_lorawan_settings.lorawan_enable)
				{
					return AT_ERRNO_PARA_VAL;
				}
				g_lorawan_settings.join_trials = nbtrials;
				lmh_setConfRetries(nbtrials);
			}
		}
		save_settings();

		if ((bJoin == 1) && !g_lorawan_initialized) // ==0 stop join, not support, yet
		{
			// Manual join only works if LoRaWAN was not initialized yet.
			// If LoRaWAN was already initialized, a restart is required
			init_lorawan();
			return AT_SUCCESS;
		}

		if ((bJoin == 1) && g_lorawan_initialized && (lmh_join_status_get() != LMH_SET))
		{
			// If if not yet joined, start join
			delay(100);
			lmh_join();
			return AT_SUCCESS;
		}

		if ((bJoin == 1) && g_lorawan_settings.auto_join)
		{
			// If auto join is set, restart the device to start join process
			delay(100);
			api_reset();
			return AT_SUCCESS;
		}
	}

	return AT_ERRNO_PARA_VAL;
}

/**
 * @brief AT+NJS Get current join status
 *
 * @return int AT_SUCCESS;
 */
static int at_query_join_status()
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	uint8_t join_status;

	join_status = (uint8_t)lmh_join_status_get();
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", join_status);

	return AT_SUCCESS;
}

/**
 * @brief AT+CFM=? Get current confirm/unconfirmed packet status
 *
 * @return int AT_SUCCESS;
 */
static int at_query_confirm(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.confirmed_msg_enabled);
	return AT_SUCCESS;
}

/**
 * @brief AT+CFM=X Set confirmed / unconfirmed packet sending
 *
 * @param str 0 = unconfirmed 1 = confirmed packet
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_confirm(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	int cfm;

	cfm = strtol(str, NULL, 0);
	if (cfm != 0 && cfm != 1)
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.confirmed_msg_enabled = (lmh_confirm)cfm;
	save_settings();

	return AT_SUCCESS;
}

/**
 * @brief AT+DR=? Get current datarate
 *
 * @return int AT_SUCCESS;
 */
static int at_query_datarate(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.data_rate);
	return AT_SUCCESS;
}

/**
 * @brief AT+DR=X Set datarate
 *
 * @param str 0 to 15, depending on region
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_datarate(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	uint8_t datarate;

	datarate = strtol(str, NULL, 0);

	if (datarate > 15)
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.data_rate = datarate;
	save_settings();

	lmh_datarate_set(datarate, g_lorawan_settings.adr_enabled);

	return AT_SUCCESS;
}

/**
 * @brief AT+ADR=? Get current adaptive datarate status
 *
 * @return int AT_SUCCESS;
 */
static int at_query_adr(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.adr_enabled ? 1 : 0);
	return AT_SUCCESS;
}

/**
 * @brief AT+ADR=X Enable/disable adaptive datarate
 *
 * @param str 0 = disable, 1 = enable ADR
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_adr(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	int adr;

	adr = strtol(str, NULL, 0);
	if (adr != 0 && adr != 1)
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.adr_enabled = (adr == 1 ? true : false);

	save_settings();

	lmh_datarate_set(g_lorawan_settings.data_rate, g_lorawan_settings.adr_enabled);

	return AT_SUCCESS;
}

/**
 * @brief AT+TXP=? Get current TX power setting
 *
 * @return int AT_SUCCESS;
 */
static int at_query_txpower(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.tx_power);
	return AT_SUCCESS;
}

/**
 * @brief AT+TXP Set TX power
 *
 * @param str TX power 0 to 10
 * @return int AT_SUCCESS;
 */
static int at_exec_txpower(char *str)
{
	if (!g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	uint8_t tx_power;

	tx_power = strtol(str, NULL, 0);

	if (tx_power > 10)
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.tx_power = tx_power;

	save_settings();

	lmh_tx_power_set(tx_power);

	return AT_SUCCESS;
}

/**
 * @brief AT+SENDINT=? Get current send frequency
 *
 * @return int AT_SUCCESS;
 */
static int at_query_sendint(void)
{
	// Return time in seconds, but it is saved in milli seconds
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", (g_lorawan_settings.send_repeat_time == 0) ? 0 : (int)(g_lorawan_settings.send_repeat_time / 1000));

	return AT_SUCCESS;
}

/**
 * @brief AT+SENDINT=<value> Set current send frequency
 *
 * @param str send frequency in seconds between 0 (disabled)
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_sendint(char *str)
{
	long time = strtoul(str, NULL, 0);

	if (time < 0)
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.send_repeat_time = time * 1000;
	save_settings();

	api_timer_restart(g_lorawan_settings.send_repeat_time);

	return AT_SUCCESS;
}

/**
 * @brief Send data packet over LoRaWAN
 *
 * @param str data packet as char array. Format <fPort>:<data>
 * 			data is in ASCII Hex format
 * @return int AT_SUCCESS if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL
 */
static int at_exec_send(char *str)
{
	if (!g_lpwan_has_joined || !g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}

	// Get fPort
	char *param;

	param = strtok(str, ":");
	uint16_t fPort = strtol(param, NULL, 0);
	if ((fPort == 0) || (fPort > 255))
	{
		return AT_ERRNO_PARA_VAL;
	}

	// Get data to send
	param = strtok(NULL, ":");
	int data_size = strlen(param);
	if (!(data_size % 2 == 0) || (data_size > 254))
	{
		return AT_ERRNO_PARA_VAL;
	}

	int buff_idx = 0;
	char buff_parse[3];
	for (int idx = 0; idx <= data_size + 1; idx += 2)
	{
		buff_parse[0] = param[idx];
		buff_parse[1] = param[idx + 1];
		buff_parse[2] = 0;
		m_lora_app_data_buffer[buff_idx] = strtol(buff_parse, NULL, 16);
		buff_idx++;
	}
	send_lora_packet(m_lora_app_data_buffer, data_size / 2, fPort);
	return AT_SUCCESS;
}

/**
 * @brief AT+BATT=? Get current battery value (0 to 255)
 *
 * @return int AT_SUCCESS;
 */
static int at_query_battery(void)
{
	// Battery level is from 1 to 254, 254 meaning fully charged.
	// snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", get_lora_batt());
	uint16_t read_val = 0;
	for (int i = 0; i < 5; i++)
	{
		read_val += read_batt();
	}
	// Battery level is in Volt
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%.2f", (float)(read_val / 5000.0));

	return AT_SUCCESS;
}

/**
 * @brief AT+RSSI=? Get RSSI of last received package
 *
 * @return int AT_SUCCESS;
 */
static int at_query_rssi(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_last_rssi);

	return AT_SUCCESS;
}

/**
 * @brief AT+SNR=? Get SNR of last received package
 *
 * @return int AT_SUCCESS;
 */
static int at_query_snr(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_last_snr);

	return AT_SUCCESS;
}

/**
 * @brief AT+VER=? Get firmware version and build date
 *
 * @return int AT_SUCCESS;
 */
static int at_query_version(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "WisBlock API %d.%d.%d", WISBLOCK_API_VER, WISBLOCK_API_VER2, WISBLOCK_API_VER3);
	return AT_SUCCESS;
}

/**
 * @brief ATZ Initiate a system reset
 *
 * @return int AT_SUCCESS;
 */
static int at_exec_reboot(void)
{
	delay(100);
	api_reset();
	return AT_SUCCESS;
}

/**
 * @brief List all device settings
 * 
 * @return int AT_SUCCESS
 */
static int at_query_status(void)
{
	at_settings();
	snprintf(g_at_query_buf, ATQUERY_SIZE, " ");

	return AT_SUCCESS;
}

/**
 * @brief ATR Restore flash defaults
 *
 * @return int AT_SUCCESS;
 */
static int at_exec_restore(void)
{
	flash_reset();
	return AT_SUCCESS;
}

/** Application build time */
const unsigned char compiler_build_time[] =
	{
		BUILD_YEAR_CH0,
		BUILD_YEAR_CH1,
		BUILD_YEAR_CH2,
		BUILD_YEAR_CH3,
		BUILD_MONTH_CH0,
		BUILD_MONTH_CH1,
		BUILD_DAY_CH0,
		BUILD_DAY_CH1,
		'-',
		BUILD_HOUR_CH0, BUILD_HOUR_CH1,
		BUILD_MIN_CH0, BUILD_MIN_CH1,
		BUILD_SEC_CH0, BUILD_SEC_CH1};

/**
 * @brief Get application build data and time
 * 
 * @return int AT_SUCCESS
 */
static int at_query_build_time(void)
{
	// snprintf(g_at_query_buf, ATQUERY_SIZE, "%d.%d.%d %s %s", g_sw_ver_1, g_sw_ver_2, g_sw_ver_3, __DATE__, __TIME__);
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%s", compiler_build_time);
	return AT_SUCCESS;
}

/**
 * @brief Get CLI version
 * 
 * @return int AT_SUCCESS
 */
static int at_query_cli(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "1.5.8");
	// snprintf(g_at_query_buf, ATQUERY_SIZE, "%d.%d.%d", WISBLOCK_API_VER, WISBLOCK_API_VER2, WISBLOCK_API_VER3);
	return AT_SUCCESS;
}

/**
 * @brief Get API version
 *
 * @return int AT_SUCCESS
 */
static int at_query_api(void)
{
	// snprintf(g_at_query_buf, ATQUERY_SIZE, "3.2.3");
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d.%d.%d", WISBLOCK_API_VER, WISBLOCK_API_VER2, WISBLOCK_API_VER3);
	return AT_SUCCESS;
}

/**
 * @brief Get HW model
 *
 * @return int AT_SUCCESS
 */
static int at_query_hw_model(void)
{
#ifdef ESP32
	snprintf(g_at_query_buf, ATQUERY_SIZE, "rak11200");
#elif defined ARDUINO_ARCH_RP2040
	snprintf(g_at_query_buf, ATQUERY_SIZE, "rak11310");
#else // NRF52_SERIES
	snprintf(g_at_query_buf, ATQUERY_SIZE, "rak4630");
#endif
	return AT_SUCCESS;
}

/**
 * @brief Get HW ID
 *
 * @return int AT_SUCCESS
 */
static int at_query_hw_id(void)
{
#ifdef ESP32
	snprintf(g_at_query_buf, ATQUERY_SIZE, "esp32");
#elif defined ARDUINO_ARCH_RP2040
	snprintf(g_at_query_buf, ATQUERY_SIZE, "rp2040");
#else // NRF52_SERIES
	snprintf(g_at_query_buf, ATQUERY_SIZE, "nrf52840");
#endif
	return AT_SUCCESS;
}

/**
 * @brief Get low power mode (always off)
 *
 * @return int AT_SUCCESS
 */
static int at_query_lpm(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "0");
	return AT_SUCCESS;
}

/**
 * @brief Get device alias (static)
 *
 * @return int AT_SUCCESS
 */
static int at_query_alias(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "WisBlock API %02X%02X%02X%02X%02X%02X%02X%02X", g_lorawan_settings.node_device_eui[0], g_lorawan_settings.node_device_eui[1],
			 g_lorawan_settings.node_device_eui[2], g_lorawan_settings.node_device_eui[3],
			 g_lorawan_settings.node_device_eui[4], g_lorawan_settings.node_device_eui[5],
			 g_lorawan_settings.node_device_eui[6], g_lorawan_settings.node_device_eui[7]);
	return AT_SUCCESS;
}

/**
 * @brief Get device serial number
 *
 * @return int AT_SUCCESS
 */
static int at_query_sn(void)
{
	uint8_t dev_sn[8] = {0};
	BoardGetUniqueId(dev_sn);
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%02X%02X%02X%02X%02X%02X%02X%02X",
			 dev_sn[7], dev_sn[6], dev_sn[5], dev_sn[4],
			 dev_sn[3], dev_sn[2], dev_sn[1], dev_sn[0]);
	return AT_SUCCESS;
}

/**
 * @brief Get LoRaWAN network ID
 *
 * @return int AT_SUCCESS
 */
static int at_query_netid(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "000000");
	return AT_SUCCESS;
}

/**
 * @brief Get confirmed result of last sent packet
 *
 * @return int AT_SUCCESS
 */
static int at_query_cfs(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_rx_fin_result ? 1 : 0);
	return AT_SUCCESS;
}

/**
 * @brief Get status of duty cycle
 *
 * @return int AT_SUCCESS
 */
static int at_query_duty(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.duty_cycle_enabled ? 1 : 0);
	return AT_SUCCESS;
}

/**
 * @brief Get status of public/private network
 *
 * @return int AT_SUCCESS
 */
static int at_query_public(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.public_network ? 1 : 0);
	return AT_SUCCESS;
}

/**
 * @brief Get last received packet
 *
 * @return int AT_SUCCESS
 */
static int at_query_recv(void)
{
	if (g_rx_data_len == 0)
	{
		snprintf(g_at_query_buf, ATQUERY_SIZE, " ");
		return AT_SUCCESS;
	}

	int len = snprintf(g_at_query_buf, ATQUERY_SIZE, "%d:", g_last_fport);
	for (int idx = 0; idx < g_rx_data_len; idx++)
	{
		snprintf(&g_at_query_buf[len], ATQUERY_SIZE, "%02X", g_rx_lora_data[idx]);
		len = len + 2;
	}
	return AT_SUCCESS;
}

/**
 * @brief Get confirmed packet retry rate
 *
 * @return int AT_SUCCESS
 */
static int at_query_retry(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", lmh_getConfRetries());
	return AT_SUCCESS;
}

/**
 * @brief Set confirmed packet retry rate
 * 
 * @param str retry rate as char array, valid values: '0' ... '8'
 * @return int 
 */
static int at_exec_retry(char *str)
{
	if (g_lorawan_settings.lorawan_enable)
	{
		return AT_ERRNO_NOALLOW;
	}
	long retries = strtol(str, NULL, 0);

	if ((retries < 0) || (retries > 8))
	{
		return AT_ERRNO_PARA_VAL;
	}

	lmh_setConfRetries(retries);
	g_lorawan_settings.join_trials = retries;
	save_settings();

	set_new_config();
	return AT_SUCCESS;
}

/**
 * @brief Get single channel usage
 * 
 * @return int AT_SUCCESS
 */
static int at_query_single(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", 0);
	return AT_SUCCESS;
}

/**
 * @brief Get eight channel usage
 *
 * @return int AT_SUCCESS
 */
static int at_query_eight(void)
{
	if ((g_lorawan_settings.lora_region == LORAMAC_REGION_US915) || (g_lorawan_settings.lora_region == LORAMAC_REGION_AU915) || (g_lorawan_settings.lora_region == LORAMAC_REGION_CN470))
	{
		uint8_t sub_channel = (g_lorawan_settings.subband_channels * 8) - 7;

		// snprintf(g_at_query_buf, ATQUERY_SIZE, "%d:%d:%d:%d:%d:%d:%d:%d", sub_channels[0], sub_channels[1], sub_channels[2], sub_channels[3], sub_channels[4], sub_channels[5], sub_channels[6], sub_channels[7]);
		snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", sub_channel);

		return AT_SUCCESS;
	}
	return AT_ERRNO_NOALLOW;
}

/**
 * @brief Get Join delay 1
 *
 * @return int AT_SUCCESS
 */
static int at_query_jdl1(void)
{
	GetPhyParams_t phy_param;
	phy_param.Attribute = PHY_JOIN_ACCEPT_DELAY1;
	PhyParam_t curr_param = RegionGetPhyParam(LoRaMacRegion, &phy_param);
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", curr_param.Value);
	return AT_SUCCESS;
}

/**
 * @brief Get Join delay 2
 *
 * @return int AT_SUCCESS
 */
static int at_query_jdl2(void)
{
	GetPhyParams_t phy_param;
	phy_param.Attribute = PHY_JOIN_ACCEPT_DELAY2;
	PhyParam_t curr_param = RegionGetPhyParam(LoRaMacRegion, &phy_param);
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", curr_param.Value);
	return AT_SUCCESS;
}

/**
 * @brief Get RX 1 delay
 *
 * @return int AT_SUCCESS
 */
static int at_query_rxdl1(void)
{
	GetPhyParams_t phy_param;
	phy_param.Attribute = PHY_RECEIVE_DELAY1;
	PhyParam_t curr_param = RegionGetPhyParam(LoRaMacRegion, &phy_param);
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", curr_param.Value);
	return AT_SUCCESS;
}

/**
 * @brief Get RX 2 delay
 *
 * @return int AT_SUCCESS
 */
static int at_query_rxdl2(void)
{
	GetPhyParams_t phy_param;
	phy_param.Attribute = PHY_RECEIVE_DELAY2;
	PhyParam_t curr_param = RegionGetPhyParam(LoRaMacRegion, &phy_param);
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", curr_param.Value);
	return AT_SUCCESS;
}

/**
 * @brief Get RX2 datarate
 *
 * @return int AT_SUCCESS
 */
static int at_query_rxdr2(void)
{
	GetPhyParams_t phy_param;
	phy_param.Attribute = PHY_DEF_RX2_DR;
	PhyParam_t curr_param = RegionGetPhyParam(LoRaMacRegion, &phy_param);
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", curr_param.Value);
	return AT_SUCCESS;
}

/**
 * @brief Get RX2 frequency
 *
 * @return int AT_SUCCESS
 */
static int at_query_rxfrq2(void)
{
	GetPhyParams_t phy_param;
	phy_param.Attribute = PHY_DEF_RX2_FREQUENCY;
	PhyParam_t curr_param = RegionGetPhyParam(LoRaMacRegion, &phy_param);
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", curr_param.Value);
	return AT_SUCCESS;
}

/**
 * @brief Get all channel RSSI (faked)
 *
 * @return int AT_SUCCESS
 */
static int at_query_arssi(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "0:%d", g_last_rssi);
	return AT_SUCCESS;
}

/**
 * @brief Get network link status (faked)
 *
 * @return int AT_SUCCESS
 */
static int at_query_link(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "0");
	return AT_SUCCESS;
}

/**
 * @brief Get multichannel settings (faked)
 *
 * @return int AT_SUCCESS
 */
static int at_query_lstmulc(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "0:00000000:00000000000000000000000000000000:00000000000000000000000000000000:000000000:00:0,0:00000000:00000000000000000000000000000000:00000000000000000000000000000000:000000000:00:0,0:00000000:00000000000000000000000000000000:00000000000000000000000000000000:000000000:00:0,0:00000000:00000000000000000000000000000000:00000000000000000000000000000000:000000000:00:0");
	return AT_SUCCESS;
}

static int at_exec_list_all(void);

/**
 * @brief List of all available commands with short help and pointer to functions
 *
 */
static atcmd_t g_at_cmd_list[] = {
	/*|    CMD    |     AT+CMD?      |    AT+CMD=?    |  AT+CMD=value |  AT+CMD  | AT Permissions |*/
	// General commands
	{"?", "AT commands", NULL, NULL, at_exec_list_all, "R"},
	{"R", "Restore default", NULL, NULL, at_exec_restore, "R"},
	{"Z", "ATZ Trig a MCU reset", NULL, NULL, at_exec_reboot, "R"},
	// LoRaWAN keys, ID's EUI's
	{"+APPEUI", "Get or set the application EUI", at_query_appeui, at_exec_appeui, NULL, "RW"},
	{"+APPKEY", "Get or set the application key", at_query_appkey, at_exec_appkey, NULL, "RW"},
	{"+DEVEUI", "Get or set the device EUI", at_query_deveui, at_exec_deveui, NULL, "RW"},
	{"+APPSKEY", "Get or set the application session key", at_query_appskey, at_exec_appskey, NULL, "RW"},
	{"+NWKSKEY", "Get or Set the network session key", at_query_nwkskey, at_exec_nwkskey, NULL, "RW"},
	{"+DEVADDR", "Get or set the device address", at_query_devaddr, at_exec_devaddr, NULL, "RW"},
	// Joining and sending data on LoRa network
	{"+CFM", "Get or set the confirm mode", at_query_confirm, at_exec_confirm, NULL, "RW"},
	{"+JOIN", "Join network", at_query_join, at_exec_join, NULL, "RW"},
	{"+NJS", "Get the join status", at_query_join_status, NULL, NULL, "R"},
	{"+NJM", "Get or set the network join mode", at_query_joinmode, at_exec_joinmode, NULL, "RW"},
	{"+SEND", "Send data", NULL, at_exec_send, NULL, "W"},
	// LoRa network management
	{"+ADR", "Get or set the adaptive data rate setting", at_query_adr, at_exec_adr, NULL, "RW"},
	{"+CLASS", "Get or set the device class", at_query_class, at_exec_class, NULL, "RW"},
	{"+DR", "Get or Set the Tx DataRate=[0..7]", at_query_datarate, at_exec_datarate, NULL, "RW"},
	{"+TXP", "Get or set the transmit power=[0...10]", at_query_txpower, at_exec_txpower, NULL, "RW"},
	{"+BAND", "Get and Set LoRaWAN region (0 = EU433, 1 = CN470, 2 = RU864, 3 = IN865, 4 = EU868, 5 = US915, 6 = AU915, 7 = KR920, 8 = AS923-1 , 9 = AS923-2 , 10 = AS923-3 , 11 = AS923-4)", at_query_region, at_exec_region, NULL, "RW"},
	{"+MASK", "Get and Set channels mask", at_query_mask, at_exec_mask, NULL, "RW"},
	// Status queries
	{"+BAT", "Get battery level", at_query_battery, NULL, NULL, "R"},
	{"+RSSI", "Last RX packet RSSI", at_query_rssi, NULL, NULL, "R"},
	{"+SNR", "Last RX packet SNR", at_query_snr, NULL, NULL, "R"},
	{"+VER", "Get SW version", at_query_version, NULL, NULL, "R"},
	// LoRa P2P management
	{"+NWM", "Switch LoRa workmode", at_query_mode, at_exec_mode, NULL, "RW"},
	{"+PFREQ", "Set P2P frequency", at_query_p2p_freq, at_exec_p2p_freq, NULL, "RW"},
	{"+PSF", "Set P2P spreading factor", at_query_p2p_sf, at_exec_p2p_sf, NULL, "RW"},
	{"+PBW", "Set P2P bandwidth", at_query_p2p_bw, at_exec_p2p_bw, NULL, "RW"},
	{"+PCR", "Set P2P coding rate", at_query_p2p_cr, at_exec_p2p_cr, NULL, "RW"},
	{"+PPL", "Set P2P preamble length", at_query_p2p_pl, at_exec_p2p_pl, NULL, "RW"},
	{"+PTP", "Set P2P TX power", at_query_p2p_txp, at_exec_p2p_txp, NULL, "RW"},
	{"+P2P", "Set P2P configuration", at_query_p2p_config, at_exec_p2p_config, NULL, "RW"},
	{"+PSEND", "P2P send data", NULL, at_exec_p2p_send, NULL, "W"},
	{"+PRECV", "P2P receive mode", at_query_p2p_receive, at_exec_p2p_receive, NULL, "RW"},
	// WisToolBox compatibility
	{"+BUILDTIME", "Get Build time", at_query_build_time, NULL, NULL, "R"},
	{"+CLIVER", "Get the version of the AT command", at_query_cli, NULL, NULL, "R"},
	{"+APIVER", "Get the version of the API", at_query_api, NULL, NULL, "R"},
	{"+HWMODEL", "Get the string of the hardware model", at_query_hw_model, NULL, NULL, "R"},
	{"+HWID", "Get the string of the MCU", at_query_hw_id, NULL, NULL, "R"},
	{"+ALIAS", "Get the device alias (na)", at_query_alias, NULL, NULL, "R"},
	{"+SN", "Get the device serial number (na)", at_query_sn, NULL, NULL, "R"},
	{"+NETID", "Get the network identifier (NetID) (3 bytes in hex)", at_query_netid, NULL, NULL, "R"},
	{"+LPM", "Get the low power mode", at_query_lpm, NULL, NULL, "R"},
	{"+CFS", "Get the last message confirm status", at_query_cfs, NULL, NULL, "R"},
	{"+DCS", "Get the duty cycle status", at_query_duty, NULL, NULL, "R"},
	{"+PNM", "Get the network mode", at_query_public, NULL, NULL, "R"},
	{"+RECV", "Get the last received packet", at_query_recv, NULL, NULL, "R"},
	{"+CHE", "Get single channel mode (na)", at_query_eight, NULL, NULL, "R"},
	{"+CHS", "Get 8 channel mode", at_query_single, NULL, NULL, "R"},
	{"+RETY", "Get the number of retries in confirmed mode", at_query_retry, at_exec_retry, NULL, "RW"},
	{"+JN1DL", "Get the join delay 1", at_query_jdl1, NULL, NULL, "R"},
	{"+JN2DL", "Get the join delay 2", at_query_jdl2, NULL, NULL, "R"},
	{"+RX1DL", "Get the RX delay 1", at_query_rxdl1, NULL, NULL, "R"},
	{"+RX2DL", "Get the RX delay 2", at_query_rxdl2, NULL, NULL, "R"},
	{"+RX2DR", "Get the RX delay 2", at_query_rxdr2, NULL, NULL, "R"},
	{"+RX2FQ", "Get the RX delay 2", at_query_rxfrq2, NULL, NULL, "R"},
	{"+ARSSI", "Get all channel RSSI", at_query_arssi, NULL, NULL, "R"},
	{"+LINKCHECK", "Get network link status", at_query_link, NULL, NULL, "R"},
	{"+LSTMULC", "Get multicast status", at_query_lstmulc, NULL, NULL, "R"},
	// Custom AT commands
	{"+STATUS", "Status, Show LoRaWAN status", at_query_status, NULL, NULL, "R"},
	{"+SENDINT", "Send interval, Get or Set the automatic send interval", at_query_sendint, at_exec_sendint, NULL, "RW"},
};
/**

// Class B or not supported by library
AT+PGSLOT=?
AT+BFREQ=?
AT+BTIME=?
AT+BGW=?
AT+LTIME=?
AT+TIMEREQ=?
AT+ENCRY=?
AT+ENCKEY=?
*/

/**
 * @brief List all available commands with short help
 *
 * @return int AT_SUCCESS;
 */
static int at_exec_list_all(void)
{
	AT_PRINTF("\r\nAT+<CMD>?: help on <CMD>");
	AT_PRINTF("AT+<CMD>: run <CMD>");
	AT_PRINTF("AT+<CMD>=<value>: set the value");
	AT_PRINTF("AT+<CMD>=?: get the value\r\n");

	for (unsigned int idx = 1; idx < sizeof(g_at_cmd_list) / sizeof(atcmd_t); idx++)
	{
		if ((strcmp(g_at_cmd_list[idx].cmd_name, (char *)"+STATUS") == 0) || (strcmp(g_at_cmd_list[idx].cmd_name, (char *)"+SENDINT") == 0))
		{
			AT_PRINTF("ATC%s,%s: %s", g_at_cmd_list[idx].cmd_name, g_at_cmd_list[idx].permission, g_at_cmd_list[idx].cmd_desc);
		}
		else
		{
			AT_PRINTF("AT%s,%s: %s", g_at_cmd_list[idx].cmd_name, g_at_cmd_list[idx].permission, g_at_cmd_list[idx].cmd_desc);
		}
	}

	if (&g_user_at_cmd_list != 0)
	{


		for (unsigned int idx = 0; idx < g_user_at_cmd_num; idx++)
		{
			AT_PRINTF("ATC%s,%s: %s", g_user_at_cmd_list[idx].cmd_name, g_user_at_cmd_list[idx].permission != NULL ? g_user_at_cmd_list[idx].permission : "RW", g_user_at_cmd_list[idx].cmd_desc);
		}
	}

	return AT_SUCCESS;
}

/**
 * @brief Handle received AT command
 *
 */
static void at_cmd_handle(void)
{
	uint8_t i;
	int ret = 0;
	const char *cmd_name;
	char *rxcmd = atcmd + 2;
	int16_t tmp = atcmd_index - 2;
	uint16_t rxcmd_index;

	if (atcmd_index < 2 || rxcmd[tmp] != '\0')
	{
		atcmd_index = 0;
		memset(atcmd, 0xff, ATCMD_SIZE);
		return;
	}

	// Serial.printf("atcmd_index==%d=%s==\n", atcmd_index, atcmd);
	if (atcmd_index == 2 && strncmp(atcmd, "AT", atcmd_index) == 0)
	{
		atcmd_index = 0;
		memset(atcmd, 0xff, ATCMD_SIZE);
		AT_PRINTF("\nOK");
		return;
	}

	rxcmd_index = tmp;

	// Check user defined AT command from list
	if (rxcmd[0] == 'C')
	{
		// RUI3 custom AT command
		// Serial.println("User CMD, remove C");
		uint8_t idx = 0;
		for (idx = 0; idx < strlen(rxcmd); idx++)
		{
			rxcmd[idx] = rxcmd[idx + 1];
		}
		rxcmd[idx] = 0;
		rxcmd_index -= 1;
	}

	// Check for standard AT commands
	for (i = 0; i < sizeof(g_at_cmd_list) / sizeof(atcmd_t); i++)
	{
		cmd_name = g_at_cmd_list[i].cmd_name;
		// Serial.printf("===rxcmd========%s================cmd_name=====%s====%d===\n", rxcmd, cmd_name, strlen(cmd_name));
		if (strlen(cmd_name) && strncmp(rxcmd, cmd_name, strlen(cmd_name)) != 0)
		{
			continue;
		}

		// Serial.printf("===rxcmd_index========%d================strlen(cmd_name)=====%d=======\n", rxcmd_index, strlen(cmd_name));

		if (rxcmd_index == (strlen(cmd_name) + 1) &&
			rxcmd[strlen(cmd_name)] == '?')
		{
			/* test cmd */
			if (g_at_cmd_list[i].cmd_desc)
			{
				if (strncmp(g_at_cmd_list[i].cmd_desc, "OK", 2) == 0)
				{
					snprintf(atcmd, ATCMD_SIZE, "\nOK");
					snprintf(cmd_result, ATCMD_SIZE," ");
				}
				else
				{
					snprintf(atcmd, ATCMD_SIZE, "\nAT%s:\"%s\"\n",
							 cmd_name, g_at_cmd_list[i].cmd_desc);
					snprintf(cmd_result, ATCMD_SIZE, "OK");
					Serial.printf(">>atcmd = %s<<\n", atcmd);
					Serial.printf(">>cmd_result = %s<<\n", cmd_result);
				}
			}
			else
			{
				snprintf(atcmd, ATCMD_SIZE, "\n%s\nOK", cmd_name);
				snprintf(cmd_result, ATCMD_SIZE, " ");
			}
		}
		else if (rxcmd_index == (strlen(cmd_name) + 2) &&
				 strcmp(&rxcmd[strlen(cmd_name)], "=?") == 0)
		{
			/* query cmd */
			if (g_at_cmd_list[i].query_cmd != NULL)
			{
				ret = g_at_cmd_list[i].query_cmd();

				if (ret == 0)
				{
					snprintf(atcmd, ATCMD_SIZE, "\nAT%s=%s\n",
							 cmd_name, g_at_query_buf);
					snprintf(cmd_result, ATCMD_SIZE, "OK");
				}
			}
			else
			{
				ret = AT_ERRNO_NOALLOW;
			}
		}
		else if (rxcmd_index > (strlen(cmd_name) + 1) &&
				 rxcmd[strlen(cmd_name)] == '=')
		{
			/* exec cmd */
			if (g_at_cmd_list[i].exec_cmd != NULL)
			{
				ret = g_at_cmd_list[i].exec_cmd(rxcmd + strlen(cmd_name) + 1);
				if (ret == 0)
				{
					snprintf(atcmd, ATCMD_SIZE, "\nOK");
					snprintf(cmd_result, ATCMD_SIZE, " ");
				}
				else if (ret == -1)
				{
					ret = AT_ERRNO_SYS;
				}
			}
			else
			{
				ret = AT_ERRNO_NOALLOW;
			}
		}
		else if (rxcmd_index == strlen(cmd_name))
		{
			/* exec cmd without parameter*/
			if (g_at_cmd_list[i].exec_cmd_no_para != NULL)
			{
				ret = g_at_cmd_list[i].exec_cmd_no_para();
				if (ret == 0)
				{
					snprintf(atcmd, ATCMD_SIZE, "\nOK");
					snprintf(cmd_result, ATCMD_SIZE, " ");
				}
				else if (ret == -1)
				{
					ret = AT_ERRNO_SYS;
				}
			}
			else
			{
				ret = AT_ERRNO_NOALLOW;
			}
		}
		else
		{
			continue;
		}
		break;
	}

#ifndef ESP32
	// ESP32 has a problem with weak declarations of functions
	if (g_user_at_cmd_list != NULL)
	{
		has_custom_at = true;
	}
#endif
	// Not a standard AT command?
	if (has_custom_at)
	{
		if (i == sizeof(g_at_cmd_list) / sizeof(atcmd_t))
		{
			// Check user defined AT command from list
			if (rxcmd[0] == 'C')
			{
				// RUI3 custom AT command
				// Serial.println("User CMD, remove C");
				uint8_t idx = 0;
				for (idx = 0; idx < strlen(rxcmd); idx++)
				{
					rxcmd[idx] = rxcmd[idx + 1];
				}
				rxcmd[idx] = 0;
				rxcmd_index -= 1;
			}
			if (g_user_at_cmd_list != NULL)
			{
				int j = 0;
				for (j = 0; j < g_user_at_cmd_num; j++)
				{
					cmd_name = g_user_at_cmd_list[j].cmd_name;
					// Serial.printf("===rxcmd========%s================cmd_name=====%s====%d===\n", rxcmd, cmd_name, strlen(cmd_name));
					if (strlen(cmd_name) && strncmp(rxcmd, cmd_name, strlen(cmd_name)) != 0)
					{
						continue;
					}

					// Serial.printf("===rxcmd_index========%d================strlen(cmd_name)=====%d=======\n", rxcmd_index, strlen(cmd_name));

					if (rxcmd_index == (strlen(cmd_name) + 1) &&
						rxcmd[strlen(cmd_name)] == '?')
					{
						/* test cmd */
						if (g_user_at_cmd_list[j].cmd_desc)
						{
							if (strncmp(g_user_at_cmd_list[j].cmd_desc, "OK", 2) == 0)
							{
								snprintf(atcmd, ATCMD_SIZE, "\nOK");
								snprintf(cmd_result, ATCMD_SIZE, " ");
							}
							else
							{
								snprintf(atcmd, ATCMD_SIZE, "\nAT%s:\"%s\"\n",
										 cmd_name, g_user_at_cmd_list[j].cmd_desc);
								snprintf(cmd_result, ATCMD_SIZE, " ");
							}
						}
						else
						{
							snprintf(atcmd, ATCMD_SIZE, "\n%s\nOK", cmd_name);
							snprintf(cmd_result, ATCMD_SIZE, " ");
						}
					}
					else if (rxcmd_index == (strlen(cmd_name) + 2) &&
							 strcmp(&rxcmd[strlen(cmd_name)], "=?") == 0)
					{
						/* query cmd */
						if (g_user_at_cmd_list[j].query_cmd != NULL)
						{
							ret = g_user_at_cmd_list[j].query_cmd();

							if (ret == 0)
							{
								snprintf(atcmd, ATCMD_SIZE, "\nAT%s=%s\n",
										 cmd_name, g_at_query_buf);
								snprintf(cmd_result, ATCMD_SIZE, "OK");
							}
						}
						else
						{
							ret = AT_ERRNO_NOALLOW;
						}
					}
					else if (rxcmd_index > (strlen(cmd_name) + 1) &&
							 rxcmd[strlen(cmd_name)] == '=')
					{
						/* exec cmd */
						if (g_user_at_cmd_list[j].exec_cmd != NULL)
						{
							ret = g_user_at_cmd_list[j].exec_cmd(rxcmd + strlen(cmd_name) + 1);
							if (ret == 0)
							{
								snprintf(atcmd, ATCMD_SIZE, "\nOK");
								snprintf(cmd_result, ATCMD_SIZE, " ");
							}
							else if (ret == -1)
							{
								ret = AT_ERRNO_SYS;
							}
						}
						else
						{
							ret = AT_ERRNO_NOALLOW;
						}
					}
					else if (rxcmd_index == strlen(cmd_name))
					{
						/* exec cmd without parameter*/
						if (g_user_at_cmd_list[j].exec_cmd_no_para != NULL)
						{
							ret = g_user_at_cmd_list[j].exec_cmd_no_para();
							if (ret == 0)
							{
								snprintf(atcmd, ATCMD_SIZE, "\nOK");
								snprintf(cmd_result, ATCMD_SIZE, " ");
							}
							else if (ret == -1)
							{
								ret = AT_ERRNO_SYS;
							}
						}
						else
						{
							ret = AT_ERRNO_NOALLOW;
						}
					}
					else
					{
						continue;
					}
					break;
				}
				if (j == g_user_at_cmd_num)
				{
					API_LOG("AT", "Not a user AT command");
					ret = AT_ERRNO_NOSUPP;
				}
			}
			// Check user AT command handler
			else if (user_at_handler != NULL)
			{
				if (user_at_handler(rxcmd, rxcmd_index))
				{
					ret = 0;
					snprintf(atcmd, ATCMD_SIZE, "\nOK");
					snprintf(cmd_result, ATCMD_SIZE, " ");
				}
				else
				{
					ret = AT_ERRNO_NOSUPP;
				}
			}
			// No user defined AT commands available
			else
			{
				ret = AT_ERRNO_NOSUPP;
			}
		}
	}

	if (ret != 0 && ret != AT_CB_PRINT)
	{
		snprintf(atcmd, ATCMD_SIZE, "\n%s%x", AT_ERROR, ret);
		snprintf(cmd_result, ATCMD_SIZE, "OK");
	}

	if (ret != AT_CB_PRINT)
	{
		AT_PRINTF(atcmd);

		AT_PRINTF(cmd_result);
	}

	atcmd_index = 0;
	memset(atcmd, 0xff, ATCMD_SIZE);
	return;
}

/**
 * @brief Get Serial input and start parsing
 *
 * @param cmd received character
 */
void at_serial_input(uint8_t cmd)
{
	Serial.printf("%c", cmd);

	// Handle backspace
	if (cmd == '\b')
	{
		atcmd[atcmd_index--] = '\0';
		Serial.printf(" \b");
	}

	// Convert to uppercase
	if (cmd >= 'a' && cmd <= 'z')
	{
		cmd = toupper(cmd);
	}

	// Check valid character
	if ((cmd >= '0' && cmd <= '9') || (cmd >= 'a' && cmd <= 'z') ||
		(cmd >= 'A' && cmd <= 'Z') || cmd == '?' || cmd == '+' || cmd == ':' ||
		cmd == '=' || cmd == ' ' || cmd == ',')
	{
		atcmd[atcmd_index++] = cmd;
	}
	else if (cmd == '\r' || cmd == '\n')
	{
		atcmd[atcmd_index] = '\0';
		at_cmd_handle();
	}

	if (atcmd_index >= ATCMD_SIZE)
	{
		atcmd_index = 0;
	}
}

#ifdef NRF52_SERIES
/**
 * @brief Callback when data over USB arrived
 *
 * @param itf unused
 */
void tud_cdc_rx_cb(uint8_t itf)
{
	g_task_event_type |= AT_CMD;
	if (g_task_sem != NULL)
	{
		xSemaphoreGiveFromISR(g_task_sem, pdFALSE);
	}
}
#endif
#ifdef ESP32
static BaseType_t xHigherPriorityTaskWoken = pdFALSE;
/**
 * @brief Callback when data over USB arrived
 */
void usb_rx_cb(void)
{
	g_task_event_type |= AT_CMD;
	if (g_task_sem != NULL)
	{
		xSemaphoreGiveFromISR(g_task_sem, &xHigherPriorityTaskWoken);
	}
}
#endif

#ifdef ARDUINO_ARCH_RP2040
/** The event handler thread */
Thread _thread_handle_serial(osPriorityNormal, 4096);

/** Thread id for lora event thread */
osThreadId _serial_task_thread = NULL;

void wb_serial_event(void)
{
	API_LOG("USB", "RX callback");
	// Notify loop task
	api_wake_loop(AT_CMD);
}

void serial1_rx_handler(void)
{
	if (_serial_task_thread != NULL)
	{
		g_task_event_type |= AT_CMD;
		osSignalSet(_serial_task_thread, AT_CMD);
	}
}

// Task to handle timer events
void _serial_task()
{
	_serial_task_thread = osThreadGetId();
	// attachInterrupt(SERIAL1_RX, serial1_rx_handler, RISING);

	// Flush for serial USB RX
	if (Serial.available())
	{
		while (Serial.available() > 0)
		{
			Serial.read();
			delay(10);
		}
	}

	// Flush for serial 1 RX
	if (Serial1.available())
	{
		while (Serial1.available() > 0)
		{
			Serial1.read();
			delay(10);
		}
	}

	while (true)
	{
		// Wait for serial USB RX
		if (Serial.available())
		{
			while (Serial.available() > 0)
			{
				at_serial_input(uint8_t(Serial.read()));
				delay(5);
			}
		}

		// Wait for serial 1 RX
		// osEvent event = osSignalWait(0, osWaitForever);
		if (Serial1.available())
		{
			while (Serial1.available() > 0)
			{
				at_serial_input(uint8_t(Serial1.read()));
				delay(5);
			}
		}
		// delay(100);
		delay(3000);
		yield();
	}
}

bool init_serial_task(void)
{
	_thread_handle_serial.start(_serial_task);
	_thread_handle_serial.set_priority(osPriorityNormal);

	/// \todo how to detect that the task is really created
	return true;
}
#endif
