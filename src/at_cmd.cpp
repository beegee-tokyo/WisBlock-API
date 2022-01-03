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
#ifdef NRF52_SERIES

#include "at_cmd.h"

#define AT_ERROR "+CME ERROR:"
#define ATCMD_SIZE 160
#define ATQUERY_SIZE 128

#define AT_ERRNO_NOSUPP (1)
#define AT_ERRNO_NOALLOW (2)
#define AT_ERRNO_PARA_VAL (5)
#define AT_ERRNO_PARA_NUM (6)
#define AT_ERRNO_SYS (8)
#define AT_CB_PRINT (0xFF)

static char atcmd[ATCMD_SIZE];
static uint16_t atcmd_index = 0;
static char g_at_query_buf[ATQUERY_SIZE];

/** LoRaWAN application data buffer. */
uint8_t m_lora_app_data_buffer[256];

char *bandwidths[] = {(char *)"125", (char *)"250", (char *)"500", (char *)"062", (char *)"041", (char *)"031", (char *)"020", (char *)"015", (char *)"010", (char *)"007"};

char *region_names[] = {(char *)"AS923", (char *)"AU915", (char *)"CN470", (char *)"CN779",
						(char *)"EU433", (char *)"EU868", (char *)"KR920", (char *)"IN865",
						(char *)"US915", (char *)"AS923-2", (char *)"AS923-3", (char *)"AS923-4", (char *)"RU864"};

typedef struct atcmd_s
{
	const char *cmd_name;		   // CMD NAME
	const char *cmd_desc;		   // AT+CMD?
	int (*query_cmd)(void);		   // AT+CMD=?
	int (*exec_cmd)(char *str);	   // AT+CMD=value
	int (*exec_cmd_no_para)(void); // AT+CMD
} atcmd_t;

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
	AT_PRINTF("Device status:\n");
	AT_PRINTF("   Auto join %s\n", g_lorawan_settings.auto_join ? "enabled" : "disabled");
	AT_PRINTF("   Mode %s\n", g_lorawan_settings.lorawan_enable ? "LPWAN" : "P2P");
	AT_PRINTF("   Network %s\n", g_lpwan_has_joined ? "joined" : "not joined");
	AT_PRINTF("LPWAN status:\n");
	AT_PRINTF("   Dev EUI %02X%02X%02X%02X%02X%02X%02X%02X\n", g_lorawan_settings.node_device_eui[0], g_lorawan_settings.node_device_eui[1],
			  g_lorawan_settings.node_device_eui[2], g_lorawan_settings.node_device_eui[3],
			  g_lorawan_settings.node_device_eui[4], g_lorawan_settings.node_device_eui[5],
			  g_lorawan_settings.node_device_eui[6], g_lorawan_settings.node_device_eui[7]);
	AT_PRINTF("   App EUI %02X%02X%02X%02X%02X%02X%02X%02X\n", g_lorawan_settings.node_app_eui[0], g_lorawan_settings.node_app_eui[1],
			  g_lorawan_settings.node_app_eui[2], g_lorawan_settings.node_app_eui[3],
			  g_lorawan_settings.node_app_eui[4], g_lorawan_settings.node_app_eui[5],
			  g_lorawan_settings.node_app_eui[6], g_lorawan_settings.node_app_eui[7]);
	AT_PRINTF("   App Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
			  g_lorawan_settings.node_app_key[0], g_lorawan_settings.node_app_key[1],
			  g_lorawan_settings.node_app_key[2], g_lorawan_settings.node_app_key[3],
			  g_lorawan_settings.node_app_key[4], g_lorawan_settings.node_app_key[5],
			  g_lorawan_settings.node_app_key[6], g_lorawan_settings.node_app_key[7],
			  g_lorawan_settings.node_app_key[8], g_lorawan_settings.node_app_key[9],
			  g_lorawan_settings.node_app_key[10], g_lorawan_settings.node_app_key[11],
			  g_lorawan_settings.node_app_key[12], g_lorawan_settings.node_app_key[13],
			  g_lorawan_settings.node_app_key[14], g_lorawan_settings.node_app_key[15]);
	AT_PRINTF("   Dev Addr %08lX\n", g_lorawan_settings.node_dev_addr);
	AT_PRINTF("   NWS Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
			  g_lorawan_settings.node_nws_key[0], g_lorawan_settings.node_nws_key[1],
			  g_lorawan_settings.node_nws_key[2], g_lorawan_settings.node_nws_key[3],
			  g_lorawan_settings.node_nws_key[4], g_lorawan_settings.node_nws_key[5],
			  g_lorawan_settings.node_nws_key[6], g_lorawan_settings.node_nws_key[7],
			  g_lorawan_settings.node_nws_key[8], g_lorawan_settings.node_nws_key[9],
			  g_lorawan_settings.node_nws_key[10], g_lorawan_settings.node_nws_key[11],
			  g_lorawan_settings.node_nws_key[12], g_lorawan_settings.node_nws_key[13],
			  g_lorawan_settings.node_nws_key[14], g_lorawan_settings.node_nws_key[15]);
	AT_PRINTF("   Apps Key %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
			  g_lorawan_settings.node_apps_key[0], g_lorawan_settings.node_apps_key[1],
			  g_lorawan_settings.node_apps_key[2], g_lorawan_settings.node_apps_key[3],
			  g_lorawan_settings.node_apps_key[4], g_lorawan_settings.node_apps_key[5],
			  g_lorawan_settings.node_apps_key[6], g_lorawan_settings.node_apps_key[7],
			  g_lorawan_settings.node_apps_key[8], g_lorawan_settings.node_apps_key[9],
			  g_lorawan_settings.node_apps_key[10], g_lorawan_settings.node_apps_key[11],
			  g_lorawan_settings.node_apps_key[12], g_lorawan_settings.node_apps_key[13],
			  g_lorawan_settings.node_apps_key[14], g_lorawan_settings.node_apps_key[15]);
	AT_PRINTF("   OTAA %s\n", g_lorawan_settings.otaa_enabled ? "enabled" : "disabled");
	AT_PRINTF("   ADR %s\n", g_lorawan_settings.adr_enabled ? "enabled" : "disabled");
	AT_PRINTF("   %s Network\n", g_lorawan_settings.public_network ? "Public" : "Private");
	AT_PRINTF("   Dutycycle %s\n", g_lorawan_settings.duty_cycle_enabled ? "enabled" : "disabled");
	AT_PRINTF("   Send Frequency %ld\n", g_lorawan_settings.send_repeat_time / 1000);
	AT_PRINTF("   Join trials %d\n", g_lorawan_settings.join_trials);
	AT_PRINTF("   TX Power %d\n", g_lorawan_settings.tx_power);
	AT_PRINTF("   DR %d\n", g_lorawan_settings.data_rate);
	AT_PRINTF("   Class %d\n", g_lorawan_settings.lora_class);
	AT_PRINTF("   Subband %d\n", g_lorawan_settings.subband_channels);
	AT_PRINTF("   Fport %d\n", g_lorawan_settings.app_port);
	AT_PRINTF("   %s Message\n", g_lorawan_settings.confirmed_msg_enabled ? "Confirmed" : "Unconfirmed");
	AT_PRINTF("   Region %s\n", region_names[g_lorawan_settings.lora_region]);
	AT_PRINTF("LoRa P2P status:\n");
	AT_PRINTF("   P2P frequency %ld\n", g_lorawan_settings.p2p_frequency);
	AT_PRINTF("   P2P TX Power %d\n", g_lorawan_settings.p2p_tx_power);
	AT_PRINTF("   P2P BW %s\n", bandwidths[g_lorawan_settings.p2p_bandwidth]);
	AT_PRINTF("   P2P SF %d\n", g_lorawan_settings.p2p_sf);
	AT_PRINTF("   P2P CR %d\n", g_lorawan_settings.p2p_cr);
	AT_PRINTF("   P2P Preamble length %d\n", g_lorawan_settings.p2p_preamble_len);
	AT_PRINTF("   P2P Symbol Timeout %d\n", g_lorawan_settings.p2p_symbol_timeout);
}

static int at_query_mode(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.lorawan_enable ? 1 : 0);
	return 0;
}

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
		sd_nvic_SystemReset();
	}
	return 0;
}

static int at_query_p2p_freq(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", g_lorawan_settings.p2p_frequency);
	return 0;
}

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
	return 0;
}

static int at_query_p2p_sf(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.p2p_sf);
	return 0;
}

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
	return 0;
}

static int at_query_p2p_bw(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%s", bandwidths[g_lorawan_settings.p2p_bandwidth]);
	return 0;
}

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
			return 0;
		}
	}
	return AT_ERRNO_PARA_VAL;
}

static int at_query_p2p_cr(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.p2p_cr);
	return 0;
}

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
	return 0;
}

static int at_query_p2p_pl(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.p2p_preamble_len);
	return 0;
}

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
	return 0;
}

static int at_query_p2p_txp(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.p2p_tx_power);
	return 0;
}

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
	return 0;
}

static int at_query_p2p_config(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld:%d:%d:%d:%d:%d",
			 g_lorawan_settings.p2p_frequency,
			 g_lorawan_settings.p2p_sf,
			 g_lorawan_settings.p2p_bandwidth,
			 g_lorawan_settings.p2p_cr,
			 g_lorawan_settings.p2p_preamble_len,
			 g_lorawan_settings.p2p_tx_power);
	return 0;
}

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
							return 0;
						}
					}
				}
			}
		}
	}
	return AT_ERRNO_PARA_NUM;
}

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
	return 0;
}

/**
 * @brief Set P2P RX mode
 * 0 => TX mode, RX disabled
 * 1 ... 65533 => Enable RX for xxxx milliseconds
 * 65534 => Enable continous RX (restarts after TX)
 * 65535 => Enable RX until a packet was received, no timeout
 * @return int 0 if no error, otherwise AT_ERRNO_NOALLOW, AT_ERRNO_PARA_VAL, AT_ERRNO_PARA_NUM
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
		return 0;
	}
	return AT_ERRNO_PARA_NUM;
}

static int at_query_p2p_receive(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%ld", g_lora_p2p_rx_time);
	return 0;
}

/**
 * @brief AT+BAND=? Get regional frequency band
 * 
 * @return int always 0
 */
static int at_query_region(void)
{
	// 0: AS923 1: AU915 2: CN470 3: CN779 4: EU433 5: EU868 6: KR920 7: IN865 8: US915 9: AS923-2 10: AS923-3 11: AS923-4 12: RU864
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.lora_region);

	return 0;
}

/**
 * @brief AT+BAND=xx Set regional frequency band
 *  Values: 0: AS923 1: AU915 2: CN470 3: CN779 4: EU433 5: EU868 6: KR920 7: IN865 8: US915 9: AS923-2 10: AS923-3 11: AS923-4 12: RU864
 * @return int 0 if valid parameter
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
		// RAK4630 0: AS923 1: AU915 2: CN470 3: CN779 4: EU433 5: EU868 6: KR920 7: IN865 8: US915 9: AS923-2 10: AS923-3 11: AS923-4 12: RU864
		if (region > 12)
		{
			return AT_ERRNO_PARA_VAL;
		}
		g_lorawan_settings.lora_region = region;
		save_settings();
	}
	else
	{
		return AT_ERRNO_PARA_VAL;
	}

	return 0;
}

/**
 * @brief AT+MASK=? Get channel mask
 *  Only available for regions 1: AU915 2: CN470 8: US915
 * @return int always 0
 */
static int at_query_mask(void)
{
	if ((g_lorawan_settings.lora_region == 1) || (g_lorawan_settings.lora_region == 2) || (g_lorawan_settings.lora_region == 8))
	{
		snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.subband_channels);

		return 0;
	}
	return AT_ERRNO_PARA_VAL;
}

/**
 * @brief AT+MASK=xx Set channel mask
 *  Only available for regions 1: AU915 2: CN470 8: US915
 * @return int 0 if valid parameter
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

	return 0;
}

/**
 * @brief AT+NJM=? Return current join modus
 * 
 * @return int always 0
 */
static int at_query_joinmode(void)
{
	int mode;
	mode = g_lorawan_settings.otaa_enabled == true ? 1 : 0;

	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", mode);
	return 0;
}

/**
 * @brief AT+NJM=x Set current join modus
 * 
 * @param str 1 = OTAA 0 = ABP
 * @return int 0 if valid parameter
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

	return 0;
}

/**
 * @brief AT+DEVEUI=? Get current Device EUI
 * 
 * @return int always 0
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
	return 0;
}

/**
 * @brief AT+DEVEUI=<XXXXXXXXXXXXXXXX> Set current Device EUI
 * @return int 0 if Dev EUI has correct length and was valid HEX
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

	return 0;
}

/**
 * @brief AT+APPEUI=? Get current Application (Join) EUI
 * 
 * @return int always 0
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
	return 0;
}

/**
 * @brief AT+APPEUI=<XXXXXXXXXXXXXXXX> Set current Application (Join) EUI
 * 
 * @return int 0 if App EUI has correct length and was valid HEX
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

	return 0;
}

/**
 * @brief AT+APPKEY=? Get current Application Key
 * 
 * @return int always 0
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
	return 0;
}

/**
 * @brief AT+APPKEY=<XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX> Set current Application (Join) EUI
 * 
 * @return int 0 if App Key has correct length and was valid HEX
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

	return 0;
}

/**
 * @brief AT+DEVADDR=? Get device address
 * 
 * @return int always 0
 */
static int at_query_devaddr(void)
{
	if (otaaDevAddr != 0)
	{
		snprintf(g_at_query_buf, ATQUERY_SIZE, "%08lX\n", otaaDevAddr);
	}
	else
	{
		snprintf(g_at_query_buf, ATQUERY_SIZE, "%08lX\n", g_lorawan_settings.node_dev_addr);
	}
	return 0;
}

/**
 * @brief AT+DEVADDR=<XXXXXXXX> Set device address
 * 
 * @return int 0 if device address has correct length and was valid HEX
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

	return 0;
}

/**
 * @brief AT+APPSKEY=? Get application session key
 * 
 * @return int always 0
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

	return 0;
}

/**
 * @brief AT+APPSKEY=<XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX> Set application session key
 * 
 * @return int 0 if App session Key has correct length and was valid HEX
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

	return 0;
}

/**
 * @brief AT+NWSKEY=? Get network session key
 * 
 * @return int always 0
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

	return 0;
}

/**
 * @brief AT+NWSKEY=<XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX> Set network session key
 * 
 * @return int 0 if Network session key has correct length and was valid HEX
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

	return 0;
}

/**
 * @brief AT+CLASS=? Get device class
 * 
 * @return int always 0
 */
static int at_query_class(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%c", g_lorawan_settings.lora_class + 65);

	return 0;
}

/**
 * @brief AT+CLASS=X Set device class
 * 
 * @param str Class A or C, B not supported
 * @return int if class was valid
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

	return 0;
}

/**
 * @brief AT+NJM=? Get join mode
 * 
 * @return int always 0
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
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d,%d,%d,%d", 0, g_lorawan_settings.auto_join, 8, g_lorawan_settings.join_trials);

	return 0;
}

/**
 * @brief AT+NJM=<Param1>,<Param2>,<Param3>,<Param4> Set join mode
 * Param1 = Join command: 1 for joining the network , 0 for stop joining (not supported)
 * Param2 = Auto-Join config: 1 for Auto-join on power up) , 0 for no auto-join.
 * Param3 = Reattempt interval: 7 - 255 seconds (ignored)
 * Param4 = No. of join attempts: 0 - 255
 * 
 * @param str parameters as string
 * @return int 0 if all parameters were valid
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
			return 0;
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
			}
		}
		save_settings();

		if ((bJoin == 1) && !g_lorawan_initialized) // ==0 stop join, not support, yet
		{
			// Manual join only works if LoRaWAN was not initialized yet.
			// If LoRaWAN was already initialized, a restart is required
			init_lorawan();
		}
		return 0;

		if ((bJoin == 1) && g_lorawan_initialized && (lmh_join_status_get() != LMH_SET))
		{
			// If if not yet joined, start join
			delay(100);
			lmh_join();
			return 0;
		}

		if ((bJoin == 1) && g_lorawan_settings.auto_join)
		{
			// If auto join is set, restart the device to start join process
			delay(100);
			sd_nvic_SystemReset();
			return 0;
		}
	}

	return AT_ERRNO_PARA_VAL;
}

/**
 * @brief AT+NJS Get current join status
 * 
 * @return int always 0
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

	return 0;
}

/**
 * @brief AT+CFM=? Get current confirm/unconfirmed packet status
 * 
 * @return int always 0
 */
static int at_query_confirm(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.confirmed_msg_enabled);
	return 0;
}

/**
 * @brief AT+CFM=X Set confirmed / unconfirmed packet sending
 * 
 * @param str 0 = unconfirmed 1 = confirmed packet
 * @return int 0 if correct parameter
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

	return 0;
}

/**
 * @brief AT+DR=? Get current datarate
 * 
 * @return int always 0
 */
static int at_query_datarate(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.data_rate);
	return 0;
}

/**
 * @brief AT+DR=X Set datarate
 * 
 * @param str 0 to 15, depending on region
 * @return int 0 if correct parameter
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

	return 0;
}

/**
 * @brief AT+ADR=? Get current adaptive datarate status
 * 
 * @return int always 0
 */
static int at_query_adr(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.adr_enabled ? 1 : 0);
	return 0;
}

/**
 * @brief AT+ADR=X Enable/disable adaptive datarate
 * 
 * @param str 0 = disable, 1 = enable ADR
 * @return int 0 if correct parameter
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

	return 0;
}

/**
 * @brief AT+TXP=? Get current TX power setting
 * 
 * @return int always 0
 */
static int at_query_txpower(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_lorawan_settings.tx_power);
	return 0;
}

/**
 * @brief AT+TXP Set TX power
 * 
 * @param str TX power 0 to 10
 * @return int always 0
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

	return 0;
}

/**
 * @brief AT+SENDFREQ=? Get current send frequency
 * 
 * @return int always 0
 */
static int at_query_sendfreq(void)
{
	// Return time in seconds, but it is saved in milli seconds
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", (g_lorawan_settings.send_repeat_time == 0) ? 0 : (int)(g_lorawan_settings.send_repeat_time / 1000));

	return 0;
}

/**
 * @brief AT+SENDFREQ=<value> Set current send frequency
 * 
 * @param str send frequency in seconds between 0 (disabled) and 3600
 * @return int 
 */
static int at_exec_sendfreq(char *str)
{
	long time = strtol(str, NULL, 0);

	if ((time < 0) || (time > 3600))
	{
		return AT_ERRNO_PARA_VAL;
	}

	g_lorawan_settings.send_repeat_time = time * 1000;
	save_settings();

	if ((g_lorawan_settings.send_repeat_time != 0) && (g_lorawan_settings.auto_join))
	{
		// Now we are connected, start the timer that will wakeup the loop frequently
		g_task_wakeup_timer.stop();
		g_task_wakeup_timer.setPeriod(g_lorawan_settings.send_repeat_time);
		g_task_wakeup_timer.start();
	}

	return 0;
}

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
	return 0;
}

/**
 * @brief AT+BATT=? Get current battery value (0 to 255)
 * 
 * @return int always 0
 */
static int at_query_battery(void)
{
	// Battery level is from 1 to 254, 254 meaning fully charged.
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", get_lora_batt());

	return 0;
}

/**
 * @brief AT+RSSI=? Get RSSI of last received package
 * 
 * @return int always 0
 */
static int at_query_rssi(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_last_rssi);

	return 0;
}

/**
 * @brief AT+SNR=? Get SNR of last received package
 * 
 * @return int always 0
 */
static int at_query_snr(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d", g_last_snr);

	return 0;
}

/**
 * @brief AT+VER=? Get firmware version and build date
 * 
 * @return int always 0
 */
static int at_query_version(void)
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "%d.%d.%d %s %s", g_sw_ver_1, g_sw_ver_2, g_sw_ver_3, __DATE__, __TIME__);

	return 0;
}

/**
 * @brief ATZ Initiate a system reset
 * 
 * @return int always 0
 */
static int at_exec_reboot(void)
{
	delay(100);
	sd_nvic_SystemReset();
	return 0;
}

static int at_query_status(void)
{
	at_settings();
	snprintf(g_at_query_buf, ATQUERY_SIZE, " ");

	return 0;
}

/**
 * @brief ATR Restore flash defaults
 * 
 * @return int always 0
 */
static int at_exec_restore(void)
{
	flash_reset();
	return 0;
}

static int at_exec_list_all(void);

/**
 * @brief List of all available commands with short help and pointer to functions
 * 
 */
static atcmd_t g_at_cmd_list[] = {
	/*|    CMD    |     AT+CMD?      |    AT+CMD=?    |  AT+CMD=value |  AT+CMD  |*/
	// General commands
	{"?", "AT commands", NULL, NULL, at_exec_list_all},
	{"R", "Restore default", NULL, NULL, at_exec_restore},
	{"Z", "ATZ Trig a MCU reset", NULL, NULL, at_exec_reboot},
	// LoRaWAN keys, ID's EUI's
	{"+APPEUI", "Get or set the application EUI", at_query_appeui, at_exec_appeui, NULL},
	{"+APPKEY", "Get or set the application key", at_query_appkey, at_exec_appkey, NULL},
	{"+DEVEUI", "Get or set the device EUI", at_query_deveui, at_exec_deveui, NULL},
	{"+APPSKEY", "Get or set the application session key", at_query_appskey, at_exec_appskey, NULL},
	{"+NWKSKEY", "Get or Set the network session key", at_query_nwkskey, at_exec_nwkskey, NULL},
	{"+DEVADDR", "Get or set the device address", at_query_devaddr, at_exec_devaddr, NULL},
	// Joining and sending data on LoRa network
	{"+CFM", "Get or set the confirm mode", at_query_confirm, at_exec_confirm, NULL},
	{"+JOIN", "Join network", at_query_join, at_exec_join, NULL},
	{"+NJS", "Get the join status", at_query_join_status, NULL, NULL},
	{"+NJM", "Get or set the network join mode", at_query_joinmode, at_exec_joinmode, NULL},
	{"+SENDFREQ", "Get or Set the automatic send time", at_query_sendfreq, at_exec_sendfreq, NULL},
	{"+SEND", "Send data", NULL, at_exec_send, NULL},
	// LoRa network management
	{"+ADR", "Get or set the adaptive data rate setting", at_query_adr, at_exec_adr, NULL},
	{"+CLASS", "Get or set the device class", at_query_class, at_exec_class, NULL},
	{"+DR", "Get or Set the Tx DataRate=[0..7]", at_query_datarate, at_exec_datarate, NULL},
	{"+TXP", "Get or set the transmit power", at_query_txpower, at_exec_txpower, NULL},
	{"+BAND", "Get and Set number corresponding to active regions", at_query_region, at_exec_region, NULL},
	{"+MASK", "Get and Set channels mask", at_query_mask, at_exec_mask, NULL},
	// Status queries
	{"+BAT", "Get battery level", at_query_battery, NULL, NULL},
	{"+RSSI", "Last RX packet RSSI", at_query_rssi, NULL, NULL},
	{"+SNR", "Last RX packet SNR", at_query_snr, NULL, NULL},
	{"+VER", "Get SW version", at_query_version, NULL, NULL},
	{"+STATUS", "Show LoRaWAN status", at_query_status, NULL, NULL},
	// LoRa P2P management
	{"+NWM", "Switch LoRa workmode", at_query_mode, at_exec_mode, NULL},
	{"+PFREQ", "Set P2P frequency", at_query_p2p_freq, at_exec_p2p_freq, NULL},
	{"+PSF", "Set P2P spreading factor", at_query_p2p_sf, at_exec_p2p_sf, NULL},
	{"+PBW", "Set P2P bandwidth", at_query_p2p_bw, at_exec_p2p_bw, NULL},
	{"+PCR", "Set P2P coding rate", at_query_p2p_cr, at_exec_p2p_cr, NULL},
	{"+PPL", "Set P2P preamble length", at_query_p2p_pl, at_exec_p2p_pl, NULL},
	{"+PTP", "Set P2P TX power", at_query_p2p_txp, at_exec_p2p_txp, NULL},
	{"+P2P", "Set P2P configuration", at_query_p2p_config, at_exec_p2p_config, NULL},
	{"+PSEND", "P2P send data", NULL, at_exec_p2p_send, NULL},
	{"+PRECV", "P2P receive mode", at_query_p2p_receive, at_exec_p2p_receive, NULL},
};

/**
 * @brief List all available commands with short help
 * 
 * @return int always 0
 */
static int at_exec_list_all(void)
{
	AT_PRINTF("\r\n+++++++++++++++\r\n");
	AT_PRINTF("AT command list\r\n");
	AT_PRINTF("+++++++++++++++\r\n");

	for (unsigned int idx = 0; idx < sizeof(g_at_cmd_list) / sizeof(atcmd_t); idx++)
	{
		if (idx < 4)
		{
			AT_PRINTF("AT%s\t\t%s\r\n", g_at_cmd_list[idx].cmd_name, g_at_cmd_list[idx].cmd_desc);
		}
		else
		{
			AT_PRINTF("AT%s\t%s\r\n", g_at_cmd_list[idx].cmd_name, g_at_cmd_list[idx].cmd_desc);
		}
	}
	AT_PRINTF("+++++++++++++++\r\n");
	return 0;
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
		AT_PRINTF("\r\nOK\r\n");
		return;
	}

	rxcmd_index = tmp;

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
					snprintf(atcmd, ATCMD_SIZE, "\r\nOK\r\n");
				}
				else
				{
					snprintf(atcmd, ATCMD_SIZE, "\r\n%s:\"%s\"\r\nOK\r\n",
							 cmd_name, g_at_cmd_list[i].cmd_desc);
				}
			}
			else
			{
				snprintf(atcmd, ATCMD_SIZE, "\r\n%s\r\nOK\r\n", cmd_name);
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
					snprintf(atcmd, ATCMD_SIZE, "\r\n%s:%s\r\nOK\r\n",
							 cmd_name, g_at_query_buf);
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
					snprintf(atcmd, ATCMD_SIZE, "\r\nOK\r\n");
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
					snprintf(atcmd, ATCMD_SIZE, "\r\nOK\r\n");
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

	// Check if user defined AT commands are setup
	if (i == sizeof(g_at_cmd_list) / sizeof(atcmd_t))
	{
		if (user_at_handler != NULL)
		{
			if (user_at_handler(rxcmd, rxcmd_index))
			{
				ret = 0;
				snprintf(atcmd, ATCMD_SIZE, "\r\nOK\r\n");
			}
			else
			{
				ret = AT_ERRNO_NOSUPP;
			}
		}
		else
		{
			ret = AT_ERRNO_NOSUPP;
		}
	}

	if (ret != 0 && ret != AT_CB_PRINT)
	{
		snprintf(atcmd, ATCMD_SIZE, "\r\n%s%x\r\n", AT_ERROR, ret);
	}

	if (ret != AT_CB_PRINT)
	{
		AT_PRINTF(atcmd);
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