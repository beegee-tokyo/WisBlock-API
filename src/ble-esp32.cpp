/**
 * @file ble-esp32.cpp
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief BLE initialization & device configuration
 * @version 0.1
 * @date 2022-06-05
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifdef ESP32

#include "WisBlock-API.h"

void start_ble_adv(void);
uint8_t pack_settings(uint8_t *buffer);
uint8_t unpack_settings(uint8_t *buffer);

// List of Service and Characteristic UUIDs
/** Service UUID for WiFi settings */
#define SERVICE_UUID "0000aaaa-ead2-11e7-80c1-9a214cf093ae"
/** Characteristic UUID for WiFi settings */
#define WIFI_UUID "00005555-ead2-11e7-80c1-9a214cf093ae"
/** Service UUID for LoRa settings */
#define LORA_UUID "f0a0"
// #define LORA_UUID "0000f0a0-ead2-11e7-80c1-9a214cf093ae"
/** Characteristic UUID for LoRa settings */
#define LPWAN_UUID "f0a1"
// #define LPWAN_UUID "0000f0a1-ead2-11e7-80c1-9a214cf093ae"
/** Service UUID for Uart */
#define UART_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
/** Characteristic UUID for receiver */
#define RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
/** Characteristic UUID for transmitter */
#define TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

/** Characteristic for digital output */
BLECharacteristic *wifi_characteristic;
/** Characteristic for digital output */
BLECharacteristic *lora_characteristic;
/** Characteristic for BLE-UART TX */
BLECharacteristic *uart_tx_characteristic;
/** Characteristic for BLE-UART RX */
BLECharacteristic *uart_rx_characteristic;
/** BLE Advertiser */
BLEAdvertising *ble_advertising;
/** BLE Service for WiFi*/
BLEService *wifi_service;
/** BLE Service for LoRa*/
BLEService *lora_service;
/** BLE Service for Uart*/
BLEService *uart_service;
/** BLE Server */
BLEServer *ble_server;

/** Buffer for JSON string */
StaticJsonDocument<512> json_buffer;

/** Flag used to detect if a BLE advertising is enabled */
bool g_ble_is_on = false;
/** Flag if device is connected */
bool g_ble_uart_is_connected = false;

Ticker advertising_timer;
Ticker blue_led_timer;
uint16_t _timeout;

void toggle_blue_led(void)
{
	digitalWrite(LED_BLUE, !digitalRead(LED_BLUE));
}

/**
 * Callbacks for client connection and disconnection
 */
class MyServerCallbacks : public BLEServerCallbacks
{
	/**
	 * Callback when a device connects
	 * @param ble_server
	 * 			Pointer to server that was connected
	 */
	void onConnect(BLEServer *ble_server)
	{
		API_LOG("BLE", "BLE client connected");
		advertising_timer.detach();
		blue_led_timer.detach();
		g_ble_uart_is_connected = true;
		digitalWrite(LED_BLUE, HIGH);
	};

	/**
	 * Callback when a device disconnects
	 * @param ble_server
	 * 			Pointer to server that was disconnected
	 */
	void onDisconnect(BLEServer *ble_server)
	{
		API_LOG("BLE", "BLE client disconnected");
		g_ble_uart_is_connected = false;
		ble_advertising->start();
		advertising_timer.once(_timeout, stop_ble_adv);
		blue_led_timer.attach(1, toggle_blue_led);
		digitalWrite(LED_BLUE, LOW);
	}
};

/**
 * Callbacks for BLE client read/write requests
 * on WiFi characteristic
 */
class WiFiCallBackHandler : public BLECharacteristicCallbacks
{
	/**
	 * Callback for write request on WiFi characteristic
	 * @param pCharacteristic
	 * 			Pointer to the characteristic
	 */
	void onWrite(BLECharacteristic *pCharacteristic)
	{
		std::string rx_value = pCharacteristic->getValue();
		if (rx_value.length() == 0)
		{
			API_LOG("BLE", "Received empty characteristic value");
			return;
		}

		// Decode data
		int key_index = 0;
		for (int index = 0; index < rx_value.length(); index++)
		{
			rx_value[index] = (char)rx_value[index] ^ (char)g_ap_name[key_index];
			key_index++;
			if (key_index >= strlen(g_ap_name))
				key_index = 0;
		}

		API_LOG("BLE", "Received data:");
#if API_LOG > 0
		for (int idx = 0; idx < rx_value.length(); idx++)
		{
			Serial.printf("%c", rx_value[idx]);
		}
		Serial.println("");
#endif

		/** Json object for incoming data */
		auto json_error = deserializeJson(json_buffer, (char *)&rx_value[0]);
		if (json_error == 0)
		{
			if (json_buffer.containsKey("ssidPrim") &&
				json_buffer.containsKey("pwPrim") &&
				json_buffer.containsKey("ssidSec") &&
				json_buffer.containsKey("pwSec"))
			{
				g_ssid_prim = json_buffer["ssidPrim"].as<String>();
				g_pw_prim = json_buffer["pwPrim"].as<String>();
				g_ssid_sec = json_buffer["ssidSec"].as<String>();
				g_pw_sec = json_buffer["pwSec"].as<String>();

				Preferences preferences;
				preferences.begin("WiFiCred", false);
				preferences.putString("g_ssid_prim", g_ssid_prim);
				preferences.putString("g_ssid_sec", g_ssid_sec);
				preferences.putString("g_pw_prim", g_pw_prim);
				preferences.putString("g_pw_sec", g_pw_sec);
				preferences.putBool("valid", true);
				if (json_buffer.containsKey("lora"))
				{
					preferences.putShort("lora", json_buffer["lora"].as<byte>());
				}
				preferences.end();

				API_LOG("BLE", "Received over Bluetooth:");
				API_LOG("BLE", "primary SSID: %s password %s", g_ssid_prim.c_str(), g_pw_prim.c_str());
				API_LOG("BLE", "secondary SSID: %s password %s", g_ssid_sec.c_str(), g_pw_sec.c_str());
				g_conn_status_changed = true;
				g_has_credentials = true;
				WiFi.disconnect(true, true);
				delay(1000);
				init_wifi();
			}
			else if (json_buffer.containsKey("erase"))
			{
				API_LOG("BLE", "Received erase command");
				WiFi.disconnect(true, true);

				Preferences preferences;
				preferences.begin("WiFiCred", false);
				preferences.clear();
				preferences.end();
				g_conn_status_changed = true;
				g_has_credentials = false;
				g_ssid_prim = "";
				g_pw_prim = "";
				g_ssid_sec = "";
				g_pw_sec = "";

				int err = nvs_flash_init();
				API_LOG("BLE", "nvs_flash_init: %d", err);
				err = nvs_flash_erase();
				API_LOG("BLE", "nvs_flash_erase: %d", err);

				esp_restart();
			}
			else if (json_buffer.containsKey("reset"))
			{
				WiFi.disconnect();
				esp_restart();
			}
			else
			{
				API_LOG("BLE", "No valid dataset");
			}
		}
		else
		{
			API_LOG("BLE", "Received invalid JSON");
		}
		json_buffer.clear();
	};

	/**
	 * Callback for read request on WiFi characteristic
	 * @param pCharacteristic
	 * 			Pointer to the characteristic
	 */
	void onRead(BLECharacteristic *pCharacteristic)
	{
		API_LOG("BLE", "WiFi BLE onRead request");
		String wifi_credentials;

		/** Json object for outgoing data */
		StaticJsonDocument<256> json_out;
		json_out["g_ssid_prim"] = g_ssid_prim;
		json_out["g_pw_prim"] = g_pw_prim;
		json_out["g_ssid_sec"] = g_ssid_sec;
		json_out["g_pw_sec"] = g_pw_sec;
		if (WiFi.isConnected())
		{
			json_out["ip"] = WiFi.localIP().toString();
			json_out["ap"] = WiFi.SSID();
		}
		else
		{
			json_out["ip"] = "0.0.0.0";
			json_out["ap"] = "";
		}

		/// \todo implement SW version as in RAK4631
		json_out["sw"] = "1.0.0";

		// Convert JSON object into a string
		serializeJson(json_out, wifi_credentials);

		// encode the data
		int key_index = 0;
		API_LOG("BLE", "Stored settings: %s", wifi_credentials.c_str());
		for (int index = 0; index < wifi_credentials.length(); index++)
		{
			wifi_credentials[index] = (char)wifi_credentials[index] ^ (char)g_ap_name[key_index];
			key_index++;
			if (key_index >= strlen(g_ap_name))
				key_index = 0;
		}
		wifi_characteristic->setValue((uint8_t *)&wifi_credentials[0], wifi_credentials.length());
		json_buffer.clear();
	}
};

/**
 * Callbacks for BLE client read/write requests
 * on LoRa characteristic
 */
class LoRaCallBackHandler : public BLECharacteristicCallbacks
{
	/**
	 * Callback for write request on LoRa characteristic
	 * @param pCharacteristic
	 * 			Pointer to the characteristic
	 */
	void onWrite(BLECharacteristic *pCharacteristic)
	{
		std::string rx_value = pCharacteristic->getValue();
		if (rx_value.length() == 0)
		{
			API_LOG("BLE", "Received empty characteristic value");
			return;
		}

		if (rx_value.length() != 88) // sizeof(s_lorawan_settings))
		{
			API_LOG("BLE", "Received settings have wrong size %d", rx_value.length());
			return;
		}

		// Save new LoRa settings
		uint8_t ble_out[sizeof(s_lorawan_settings)];
		memcpy(ble_out, &rx_value[0], 89);

		if ((ble_out[0] != 0xAA) || (ble_out[1] != LORAWAN_DATA_MARKER))
		{
			API_LOG("BLE", "Received settings data do not have required markers");
			API_LOG("BLE", "Marker 1 %02X Marker 2 %02X", ble_out[0], ble_out[1]);
			return;
		}

		uint8_t size = unpack_settings(ble_out);
		Serial.printf("Size: %d\n", size);
		Serial.printf("Region: %d\n", (uint8_t)rx_value[87]);

		// Save new settings
		save_settings();

		log_settings();

		if (g_lorawan_settings.resetRequest)
		{
			API_LOG("BLE", "Initiate reset");
			delay(1000);
			esp_restart();
		}
	};

	/**
	 * Callback for read request on LoRa characteristic
	 * @param pCharacteristic
	 * 			Pointer to the characteristic
	 */
	void onRead(BLECharacteristic *pCharacteristic)
	{
		API_LOG("BLE", "BLE onRead request");

		uint8_t ble_out[sizeof(s_lorawan_settings)];
		uint8_t packet_len = pack_settings(ble_out);

		Serial.printf("%0X%0X\n", ble_out[0], ble_out[1]);
		Serial.printf("Size: %d\n", packet_len);
		lora_characteristic->setValue(ble_out, packet_len);
	}
};

/**
 * Callbacks for BLE UART write requests
 * on WiFi characteristic
 */
class UartCallBackHandler : public BLECharacteristicCallbacks
{
	/**
	 * Callback for write request on UART characteristic
	 * @param pCharacteristic
	 * 			Pointer to the characteristic
	 */
	void onWrite(BLECharacteristic *ble_characteristic)
	{
		std::string rx_value = ble_characteristic->getValue();

		if (rx_value.length() > 0)
		{
			API_LOG("BLE", "Received Value: %s", rx_value.c_str());
			for (int idx = 0; idx < rx_value.length(); idx++)
			{
				at_serial_input(rx_value[idx]);
			}
			at_serial_input('\r');
		}
	}
};

/**
 * Initialize BLE service and characteristic
 * Start BLE server and service advertising
 */
void init_ble()
{
	// Create device name
	char helper_string[256] = {0};
	sprintf(helper_string, "%s-%02X%02X%02X%02X%02X%02X", g_ble_dev_name,
			(uint8_t)(g_lorawan_settings.node_device_eui[2]), (uint8_t)(g_lorawan_settings.node_device_eui[3]),
			(uint8_t)(g_lorawan_settings.node_device_eui[4]), (uint8_t)(g_lorawan_settings.node_device_eui[5]), (uint8_t)(g_lorawan_settings.node_device_eui[6]), (uint8_t)(g_lorawan_settings.node_device_eui[7]));

	API_LOG("BLE", "Initialize BLE");
	// Initialize BLE and set output power
	BLEDevice::init(g_ble_dev_name);
	BLEDevice::setPower(ESP_PWR_LVL_P7);
	BLEDevice::setMTU(200);

	BLEAddress thisAddress = BLEDevice::getAddress();

	API_LOG("BLE", "BLE address: %s\n", thisAddress.toString().c_str());

	// Create BLE Server
	ble_server = BLEDevice::createServer();

	// Set server callbacks
	ble_server->setCallbacks(new MyServerCallbacks());

	// Create WiFi BLE Service
	wifi_service = ble_server->createService(BLEUUID(SERVICE_UUID));

	// Create BLE Characteristic for WiFi settings
	wifi_characteristic = wifi_service->createCharacteristic(
		BLEUUID(WIFI_UUID),
		NIMBLE_PROPERTY::READ |
			NIMBLE_PROPERTY::WRITE);
	wifi_characteristic->setCallbacks(new WiFiCallBackHandler());

	// Start the service
	wifi_service->start();

	// Create LoRa BLE Service
	lora_service = ble_server->createService(BLEUUID(LORA_UUID));

	// Create BLE Characteristic for WiFi settings
	lora_characteristic = lora_service->createCharacteristic(
		BLEUUID(LPWAN_UUID),
		NIMBLE_PROPERTY::READ |
			NIMBLE_PROPERTY::WRITE);
	lora_characteristic->setCallbacks(new LoRaCallBackHandler());

	// Start the service
	lora_service->start();

	// Create the UART BLE Service
	uart_service = ble_server->createService(UART_UUID);

	// Create a BLE Characteristic
	uart_tx_characteristic = uart_service->createCharacteristic(
		TX_UUID,
		NIMBLE_PROPERTY::NOTIFY);

	uart_rx_characteristic = uart_service->createCharacteristic(
		RX_UUID,
		NIMBLE_PROPERTY::WRITE);

	uart_rx_characteristic->setCallbacks(new UartCallBackHandler());

	// Start the service
	uart_service->start();

	// Start advertising
	ble_advertising = ble_server->getAdvertising();
	ble_advertising->addServiceUUID(SERVICE_UUID);
	ble_advertising->addServiceUUID(LORA_UUID);
	ble_advertising->addServiceUUID(UART_UUID);
	start_ble_adv();
}

void restart_advertising(uint16_t timeout)
{
	_timeout = timeout;
	if (timeout != 0)
	{
		advertising_timer.once(timeout, stop_ble_adv);
		blue_led_timer.attach(1, toggle_blue_led);
	}
	ble_advertising->start(timeout);
	g_ble_is_on = true;
}

/**
 * Stop BLE advertising
 */
void stop_ble_adv(void)
{
	advertising_timer.detach();
	blue_led_timer.detach();
	digitalWrite(LED_BLUE, LOW);
	/// \todo needs patch in BLEAdvertising.cpp -> handleGAPEvent() -> remove start(); from ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT
	ble_advertising->stop();
	g_ble_is_on = false;
}

/**
 * Start BLE advertising
 */
void start_ble_adv(void)
{
	blue_led_timer.attach(1, toggle_blue_led);
	if (g_lorawan_settings.auto_join)
	{
		advertising_timer.once(60, stop_ble_adv);
		ble_advertising->start(60);
	}
	else
	{
		ble_advertising->start(0);
	}
	g_ble_is_on = true;
}

/**
 * @brief Pack settings for BLE transmission
 *
 * @param buffer Buffer to write the packed data into
 * @return uint8_t number of bytes written into buffer
 */
uint8_t pack_settings(uint8_t *buffer)
{
	int i = 0;
	buffer[i++] = g_lorawan_settings.valid_mark_1;
	buffer[i++] = g_lorawan_settings.valid_mark_2;
	buffer[i++] = g_lorawan_settings.node_device_eui[0];
	buffer[i++] = g_lorawan_settings.node_device_eui[1];
	buffer[i++] = g_lorawan_settings.node_device_eui[2];
	buffer[i++] = g_lorawan_settings.node_device_eui[3];
	buffer[i++] = g_lorawan_settings.node_device_eui[4];
	buffer[i++] = g_lorawan_settings.node_device_eui[5];
	buffer[i++] = g_lorawan_settings.node_device_eui[6];
	buffer[i++] = g_lorawan_settings.node_device_eui[7];
	buffer[i++] = g_lorawan_settings.node_app_eui[0];
	buffer[i++] = g_lorawan_settings.node_app_eui[1];
	buffer[i++] = g_lorawan_settings.node_app_eui[2];
	buffer[i++] = g_lorawan_settings.node_app_eui[3];
	buffer[i++] = g_lorawan_settings.node_app_eui[4];
	buffer[i++] = g_lorawan_settings.node_app_eui[5];
	buffer[i++] = g_lorawan_settings.node_app_eui[6];
	buffer[i++] = g_lorawan_settings.node_app_eui[7];
	buffer[i++] = g_lorawan_settings.node_app_key[0];
	buffer[i++] = g_lorawan_settings.node_app_key[1];
	buffer[i++] = g_lorawan_settings.node_app_key[2];
	buffer[i++] = g_lorawan_settings.node_app_key[3];
	buffer[i++] = g_lorawan_settings.node_app_key[4];
	buffer[i++] = g_lorawan_settings.node_app_key[5];
	buffer[i++] = g_lorawan_settings.node_app_key[6];
	buffer[i++] = g_lorawan_settings.node_app_key[7];
	buffer[i++] = g_lorawan_settings.node_app_key[8];
	buffer[i++] = g_lorawan_settings.node_app_key[9];
	buffer[i++] = g_lorawan_settings.node_app_key[10];
	buffer[i++] = g_lorawan_settings.node_app_key[11];
	buffer[i++] = g_lorawan_settings.node_app_key[12];
	buffer[i++] = g_lorawan_settings.node_app_key[13];
	buffer[i++] = g_lorawan_settings.node_app_key[14];
	buffer[i++] = g_lorawan_settings.node_app_key[15];
	buffer[i++] = (uint8_t)(g_lorawan_settings.node_dev_addr);
	buffer[i++] = (uint8_t)(g_lorawan_settings.node_dev_addr >> 8);
	buffer[i++] = (uint8_t)(g_lorawan_settings.node_dev_addr >> 16);
	buffer[i++] = (uint8_t)(g_lorawan_settings.node_dev_addr >> 24);
	buffer[i++] = g_lorawan_settings.node_nws_key[0];
	buffer[i++] = g_lorawan_settings.node_nws_key[1];
	buffer[i++] = g_lorawan_settings.node_nws_key[2];
	buffer[i++] = g_lorawan_settings.node_nws_key[3];
	buffer[i++] = g_lorawan_settings.node_nws_key[4];
	buffer[i++] = g_lorawan_settings.node_nws_key[5];
	buffer[i++] = g_lorawan_settings.node_nws_key[6];
	buffer[i++] = g_lorawan_settings.node_nws_key[7];
	buffer[i++] = g_lorawan_settings.node_nws_key[8];
	buffer[i++] = g_lorawan_settings.node_nws_key[9];
	buffer[i++] = g_lorawan_settings.node_nws_key[10];
	buffer[i++] = g_lorawan_settings.node_nws_key[11];
	buffer[i++] = g_lorawan_settings.node_nws_key[12];
	buffer[i++] = g_lorawan_settings.node_nws_key[13];
	buffer[i++] = g_lorawan_settings.node_nws_key[14];
	buffer[i++] = g_lorawan_settings.node_nws_key[15];
	buffer[i++] = g_lorawan_settings.node_apps_key[0];
	buffer[i++] = g_lorawan_settings.node_apps_key[1];
	buffer[i++] = g_lorawan_settings.node_apps_key[2];
	buffer[i++] = g_lorawan_settings.node_apps_key[3];
	buffer[i++] = g_lorawan_settings.node_apps_key[4];
	buffer[i++] = g_lorawan_settings.node_apps_key[5];
	buffer[i++] = g_lorawan_settings.node_apps_key[6];
	buffer[i++] = g_lorawan_settings.node_apps_key[7];
	buffer[i++] = g_lorawan_settings.node_apps_key[8];
	buffer[i++] = g_lorawan_settings.node_apps_key[9];
	buffer[i++] = g_lorawan_settings.node_apps_key[10];
	buffer[i++] = g_lorawan_settings.node_apps_key[11];
	buffer[i++] = g_lorawan_settings.node_apps_key[12];
	buffer[i++] = g_lorawan_settings.node_apps_key[13];
	buffer[i++] = g_lorawan_settings.node_apps_key[14];
	buffer[i++] = g_lorawan_settings.node_apps_key[15];
	buffer[i++] = g_lorawan_settings.otaa_enabled;
	buffer[i++] = g_lorawan_settings.adr_enabled;
	buffer[i++] = g_lorawan_settings.public_network;
	buffer[i++] = g_lorawan_settings.duty_cycle_enabled;
	buffer[i++] = (uint8_t)(g_lorawan_settings.send_repeat_time);
	buffer[i++] = (uint8_t)(g_lorawan_settings.send_repeat_time >> 8);
	buffer[i++] = (uint8_t)(g_lorawan_settings.send_repeat_time >> 16);
	buffer[i++] = (uint8_t)(g_lorawan_settings.send_repeat_time >> 24);
	buffer[i++] = g_lorawan_settings.join_trials;
	buffer[i++] = g_lorawan_settings.tx_power;
	buffer[i++] = g_lorawan_settings.data_rate;
	buffer[i++] = g_lorawan_settings.lora_class;
	buffer[i++] = g_lorawan_settings.subband_channels;
	buffer[i++] = g_lorawan_settings.auto_join;
	buffer[i++] = g_lorawan_settings.app_port;
	buffer[i++] = g_lorawan_settings.confirmed_msg_enabled;
	buffer[i++] = g_lorawan_settings.lora_region;

	buffer[i++] = g_lorawan_settings.lorawan_enable;
	buffer[i++] = (uint8_t)(g_lorawan_settings.p2p_frequency);
	buffer[i++] = (uint8_t)(g_lorawan_settings.p2p_frequency >> 8);
	buffer[i++] = (uint8_t)(g_lorawan_settings.p2p_frequency >> 16);
	buffer[i++] = (uint8_t)(g_lorawan_settings.p2p_frequency >> 24);
	buffer[i++] = g_lorawan_settings.p2p_tx_power;
	buffer[i++] = g_lorawan_settings.p2p_bandwidth;
	buffer[i++] = g_lorawan_settings.p2p_sf;
	buffer[i++] = g_lorawan_settings.p2p_cr;
	buffer[i++] = g_lorawan_settings.p2p_preamble_len;
	buffer[i++] = g_lorawan_settings.p2p_symbol_timeout;

	return i + 1;
}

/**
 * @brief Unpack received settings
 *
 * @param buffer Buffer with received settings
 * @return uint8_t number of bytes handled
 */
uint8_t unpack_settings(uint8_t *buffer)
{
	int i = 0;
	g_lorawan_settings.valid_mark_1 = buffer[i++];
	g_lorawan_settings.valid_mark_2 = buffer[i++];
	g_lorawan_settings.node_device_eui[0] = buffer[i++];
	g_lorawan_settings.node_device_eui[1] = buffer[i++];
	g_lorawan_settings.node_device_eui[2] = buffer[i++];
	g_lorawan_settings.node_device_eui[3] = buffer[i++];
	g_lorawan_settings.node_device_eui[4] = buffer[i++];
	g_lorawan_settings.node_device_eui[5] = buffer[i++];
	g_lorawan_settings.node_device_eui[6] = buffer[i++];
	g_lorawan_settings.node_device_eui[7] = buffer[i++];
	g_lorawan_settings.node_app_eui[0] = buffer[i++];
	g_lorawan_settings.node_app_eui[1] = buffer[i++];
	g_lorawan_settings.node_app_eui[2] = buffer[i++];
	g_lorawan_settings.node_app_eui[3] = buffer[i++];
	g_lorawan_settings.node_app_eui[4] = buffer[i++];
	g_lorawan_settings.node_app_eui[5] = buffer[i++];
	g_lorawan_settings.node_app_eui[6] = buffer[i++];
	g_lorawan_settings.node_app_eui[7] = buffer[i++];
	g_lorawan_settings.node_app_key[0] = buffer[i++];
	g_lorawan_settings.node_app_key[1] = buffer[i++];
	g_lorawan_settings.node_app_key[2] = buffer[i++];
	g_lorawan_settings.node_app_key[3] = buffer[i++];
	g_lorawan_settings.node_app_key[4] = buffer[i++];
	g_lorawan_settings.node_app_key[5] = buffer[i++];
	g_lorawan_settings.node_app_key[6] = buffer[i++];
	g_lorawan_settings.node_app_key[7] = buffer[i++];
	g_lorawan_settings.node_app_key[8] = buffer[i++];
	g_lorawan_settings.node_app_key[9] = buffer[i++];
	g_lorawan_settings.node_app_key[10] = buffer[i++];
	g_lorawan_settings.node_app_key[11] = buffer[i++];
	g_lorawan_settings.node_app_key[12] = buffer[i++];
	g_lorawan_settings.node_app_key[13] = buffer[i++];
	g_lorawan_settings.node_app_key[14] = buffer[i++];
	g_lorawan_settings.node_app_key[15] = buffer[i++];
	g_lorawan_settings.node_dev_addr = ((uint32_t)(buffer[i++]));
	g_lorawan_settings.node_dev_addr = g_lorawan_settings.node_dev_addr | ((uint32_t)(buffer[i++] << 8));
	g_lorawan_settings.node_dev_addr = g_lorawan_settings.node_dev_addr | ((uint32_t)(buffer[i++] << 16));
	g_lorawan_settings.node_dev_addr = g_lorawan_settings.node_dev_addr | ((uint32_t)(buffer[i++] << 24));
	g_lorawan_settings.node_nws_key[0] = buffer[i++];
	g_lorawan_settings.node_nws_key[1] = buffer[i++];
	g_lorawan_settings.node_nws_key[2] = buffer[i++];
	g_lorawan_settings.node_nws_key[3] = buffer[i++];
	g_lorawan_settings.node_nws_key[4] = buffer[i++];
	g_lorawan_settings.node_nws_key[5] = buffer[i++];
	g_lorawan_settings.node_nws_key[6] = buffer[i++];
	g_lorawan_settings.node_nws_key[7] = buffer[i++];
	g_lorawan_settings.node_nws_key[8] = buffer[i++];
	g_lorawan_settings.node_nws_key[9] = buffer[i++];
	g_lorawan_settings.node_nws_key[10] = buffer[i++];
	g_lorawan_settings.node_nws_key[11] = buffer[i++];
	g_lorawan_settings.node_nws_key[12] = buffer[i++];
	g_lorawan_settings.node_nws_key[13] = buffer[i++];
	g_lorawan_settings.node_nws_key[14] = buffer[i++];
	g_lorawan_settings.node_nws_key[15] = buffer[i++];
	g_lorawan_settings.node_apps_key[0] = buffer[i++];
	g_lorawan_settings.node_apps_key[1] = buffer[i++];
	g_lorawan_settings.node_apps_key[2] = buffer[i++];
	g_lorawan_settings.node_apps_key[3] = buffer[i++];
	g_lorawan_settings.node_apps_key[4] = buffer[i++];
	g_lorawan_settings.node_apps_key[5] = buffer[i++];
	g_lorawan_settings.node_apps_key[6] = buffer[i++];
	g_lorawan_settings.node_apps_key[7] = buffer[i++];
	g_lorawan_settings.node_apps_key[8] = buffer[i++];
	g_lorawan_settings.node_apps_key[9] = buffer[i++];
	g_lorawan_settings.node_apps_key[10] = buffer[i++];
	g_lorawan_settings.node_apps_key[11] = buffer[i++];
	g_lorawan_settings.node_apps_key[12] = buffer[i++];
	g_lorawan_settings.node_apps_key[13] = buffer[i++];
	g_lorawan_settings.node_apps_key[14] = buffer[i++];
	g_lorawan_settings.node_apps_key[15] = buffer[i++];
	g_lorawan_settings.otaa_enabled = buffer[i++];
	g_lorawan_settings.adr_enabled = buffer[i++];
	g_lorawan_settings.public_network = buffer[i++];
	g_lorawan_settings.duty_cycle_enabled = buffer[i++];
	g_lorawan_settings.send_repeat_time = ((uint32_t)(buffer[i++]));
	g_lorawan_settings.send_repeat_time = g_lorawan_settings.send_repeat_time | ((uint32_t)(buffer[i++] << 8));
	g_lorawan_settings.send_repeat_time = g_lorawan_settings.send_repeat_time | ((uint32_t)(buffer[i++] << 16));
	g_lorawan_settings.send_repeat_time = g_lorawan_settings.send_repeat_time | ((uint32_t)(buffer[i++] << 24));
	g_lorawan_settings.join_trials = buffer[i++];
	g_lorawan_settings.tx_power = buffer[i++];
	g_lorawan_settings.data_rate = buffer[i++];
	g_lorawan_settings.lora_class = buffer[i++];
	g_lorawan_settings.subband_channels = buffer[i++];
	g_lorawan_settings.auto_join = buffer[i++];
	g_lorawan_settings.app_port = buffer[i++];
	g_lorawan_settings.confirmed_msg_enabled = (lmh_confirm)buffer[i++];
	g_lorawan_settings.lora_region = buffer[i++];

	g_lorawan_settings.lorawan_enable = buffer[i++];
	g_lorawan_settings.p2p_frequency = ((uint32_t)(buffer[i++]));
	g_lorawan_settings.p2p_frequency = g_lorawan_settings.send_repeat_time | ((uint32_t)(buffer[i++] << 8));
	g_lorawan_settings.p2p_frequency = g_lorawan_settings.send_repeat_time | ((uint32_t)(buffer[i++] << 16));
	g_lorawan_settings.p2p_frequency = g_lorawan_settings.send_repeat_time | ((uint32_t)(buffer[i++] << 24));
	g_lorawan_settings.p2p_tx_power = buffer[i++];
	g_lorawan_settings.p2p_bandwidth = buffer[i++];
	g_lorawan_settings.p2p_sf = buffer[i++];
	g_lorawan_settings.p2p_cr = buffer[i++];
	g_lorawan_settings.p2p_preamble_len = buffer[i++];
	g_lorawan_settings.p2p_symbol_timeout = buffer[i++];

	return i + 1;
}

#endif // ESP32