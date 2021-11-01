/**
 * @file SX126x-API.h
 * @author Bernd Giesecke (bernd.giesecke@rakwireless.com)
 * @brief Includes and global declarations
 * @version 0.1
 * @date 2021-01-10
 * 
 * @copyright Copyright (c) 2021
 * 
 */
#ifdef NRF52_SERIES

#ifndef SX126x_API_H
#define SX126x_API_H

#ifndef NO_BLE_LED
// Set usage of BLE connection LED (blue). Comment the line to enable LED
#define NO_BLE_LED 1
#endif

// Debug output set to 0 to disable app debug output
#ifndef API_DEBUG
#define API_DEBUG 0
#endif

#if API_DEBUG > 0
#define API_LOG(tag, ...)           \
	do                            \
	{                             \
		if (tag)                  \
			PRINTF("[%s] ", tag); \
		PRINTF(__VA_ARGS__);      \
		PRINTF("\n");             \
	} while (0)
#else
#define API_LOG(...)
#endif

#include <Arduino.h>
#include <nrf_nvic.h>

// Main loop stuff
void periodic_wakeup(TimerHandle_t unused);
extern SemaphoreHandle_t g_task_sem;
extern volatile uint16_t g_task_event_type;
extern SoftwareTimer g_task_wakeup_timer;

/** Wake up events, more events can be defined in app.h */
#define NO_EVENT 0
#define STATUS          0b0000000000000001
#define N_STATUS        0b1111111111111110
#define BLE_CONFIG      0b0000000000000010
#define N_BLE_CONFIG    0b1111111111111101
#define BLE_DATA        0b0000000000000100
#define N_BLE_DATA      0b1111111111111011
#define LORA_DATA       0b0000000000001000
#define N_LORA_DATA     0b1111111111110111
#define LORA_TX_FIN     0b0000000000010000
#define N_LORA_TX_FIN   0b1111111111101111
#define AT_CMD          0b0000000000100000
#define N_AT_CMD        0b1111111111011111
#define LORA_JOIN_FIN   0b0000000001000000
#define N_LORA_JOIN_FIN 0b1111111110111111

// BLE
#include <bluefruit.h>
void init_ble(void);
BLEService init_settings_characteristic(void);
void restart_advertising(uint16_t timeout);
extern BLECharacteristic g_lora_data;
extern BLEUart g_ble_uart;
extern bool g_ble_uart_is_connected;
extern char g_ble_dev_name[];
extern bool g_enable_ble;

// LoRa
#include <LoRaWan-Arduino.h>

int8_t init_lora(void);
lmh_error_status send_lora_packet(uint8_t *data, uint8_t size, uint8_t fport = 0);
extern bool g_lpwan_has_joined;
extern bool g_rx_fin_result;
extern bool g_join_result;
extern uint32_t otaaDevAddr;

#define LORAWAN_DATA_MARKER 0x57
struct s_lorawan_settings
{
	uint8_t valid_mark_1 = 0xAA;				// Just a marker for the Flash
	uint8_t valid_mark_2 = LORAWAN_DATA_MARKER; // Just a marker for the Flash

	// Flag if node joins automatically after reboot
	bool auto_join = false;
	// Flag for OTAA or ABP
	bool otaa_enabled = true;
	// OTAA Device EUI MSB
	uint8_t node_device_eui[8] = {0x00, 0x0D, 0x75, 0xE6, 0x56, 0x4D, 0xC1, 0xF3};
	// OTAA Application EUI MSB
	uint8_t node_app_eui[8] = {0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x02, 0x01, 0xE1};
	// OTAA Application Key MSB
	uint8_t node_app_key[16] = {0x2B, 0x84, 0xE0, 0xB0, 0x9B, 0x68, 0xE5, 0xCB, 0x42, 0x17, 0x6F, 0xE7, 0x53, 0xDC, 0xEE, 0x79};
	// ABP Network Session Key MSB
	uint8_t node_nws_key[16] = {0x32, 0x3D, 0x15, 0x5A, 0x00, 0x0D, 0xF3, 0x35, 0x30, 0x7A, 0x16, 0xDA, 0x0C, 0x9D, 0xF5, 0x3F};
	// ABP Application Session key MSB
	uint8_t node_apps_key[16] = {0x3F, 0x6A, 0x66, 0x45, 0x9D, 0x5E, 0xDC, 0xA6, 0x3C, 0xBC, 0x46, 0x19, 0xCD, 0x61, 0xA1, 0x1E};
	// ABP Device Address MSB
	uint32_t node_dev_addr = 0x26021FB4;
	// Send repeat time in milliseconds: 2 * 60 * 1000 => 2 minutes
	uint32_t send_repeat_time = 0;
	// Flag for ADR on or off
	bool adr_enabled = false;
	// Flag for public or private network
	bool public_network = true;
	// Flag to enable duty cycle (validity depends on Region)
	bool duty_cycle_enabled = false;
	// Number of join retries
	uint8_t join_trials = 5;
	// TX power 0 .. 15 (validity depends on Region)
	uint8_t tx_power = 0;
	// Data rate 0 .. 15 (validity depends on Region)
	uint8_t data_rate = 3;
	// LoRaWAN class 0: A, 2: C, 1: B is not supported
	uint8_t lora_class = 0;
	// Subband channel selection 1 .. 9
	uint8_t subband_channels = 1;
	// Data port to send data
	uint8_t app_port = 2;
	// Flag to enable confirmed messages
	lmh_confirm confirmed_msg_enabled = LMH_UNCONFIRMED_MSG;
	// Command from BLE to reset device
	bool resetRequest = true;
	// LoRa region
	uint8_t lora_region = 0;
};

// int size = sizeof(s_lorawan_settings);
extern s_lorawan_settings g_lorawan_settings;
extern uint8_t g_rx_lora_data[];
extern uint8_t g_rx_data_len;
extern bool g_lorawan_initialized;
extern int16_t g_last_rssi;
extern int8_t g_last_snr;
extern uint8_t g_last_fport;

// Flash
void init_flash(void);
bool save_settings(void);
void log_settings(void);
void flash_reset(void);

// Battery
void init_batt(void);
float read_batt(void);
uint8_t get_lora_batt(void);
uint8_t mv_to_percent(float mvolts);

// AT command parser
void at_serial_input(uint8_t cmd);

// API stuff
void setup_app(void);
bool init_app(void);
void app_event_handler(void);
void ble_data_handler(void) __attribute__((weak));
void lora_data_handler(void);

void api_set_version(uint16_t sw_1 = 1, uint16_t sw_2 = 0, uint16_t sw_3 = 0);
void api_read_credentials(void);
void api_set_credentials(void);
extern uint16_t g_sw_ver_1; // major version increase on API change / not backwards compatible
extern uint16_t g_sw_ver_2; // minor version increase on API change / backward compatible
extern uint16_t g_sw_ver_3; // patch version increase on bugfix, no affect on API

#endif // SX126x-API_H

#endif // NRF52_SERIES
