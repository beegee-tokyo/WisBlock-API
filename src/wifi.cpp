/**
 * @file wifi.cpp
 * @author Bernd Giesecke (bernd@giesecke.tk)
 * @brief WiFi initialisation and callback handlers
 * @version 0.1
 * @date 2022-06-05
 *
 * @copyright Copyright (c) 2022
 *
 */
#ifdef ESP32
#include "WisBlock-API.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_wifi.h>

/** Unique device name */
char g_ap_name[] = "MHC-SMA-XXXXXXXXXXXXXXXX";
// const char *devAddr;
uint8_t devAddrArray[8];

/** Selected network
	true = use primary network
	false = use secondary network
*/
bool usePrimAP = true;
/** Flag if stored AP credentials are available */
bool g_has_credentials = false;
/** Connection status */
volatile bool g_wifi_connected = false;
/** Connection change status */
bool g_conn_status_changed = false;

/** Multi WiFi */
WiFiMulti wifi_multi;

/** Primary SSID of local WiFi network */
String g_ssid_prim;
/** Secondary SSID of local WiFi network */
String g_ssid_sec;
/** Password for primary local WiFi network */
String g_pw_prim;
/** Password for secondary local WiFi network */
String g_pw_sec;

/**
 * @briefCallback for WiFi events
 */
void wifi_event_cb(WiFiEvent_t event)
{
	API_LOG("WiFi", "event: %d", event);
	IPAddress localIP;
	switch (event)
	{
	case SYSTEM_EVENT_STA_GOT_IP:
		g_conn_status_changed = true;

		localIP = WiFi.localIP();
		API_LOG("WiFi", "Connected to AP: %s with IP: %d.%d.%d.%d RSSI: %d",
				WiFi.SSID().c_str(),
				localIP[0], localIP[1], localIP[2], localIP[3],
				WiFi.RSSI());
		g_wifi_connected = true;
		break;
	case SYSTEM_EVENT_STA_DISCONNECTED:
		g_conn_status_changed = true;
		API_LOG("WiFi", "WiFi lost connection");
		g_wifi_connected = false;
		break;
	case SYSTEM_EVENT_SCAN_DONE:
		API_LOG("WiFi", "WiFi scan finished");
		break;
	case SYSTEM_EVENT_STA_CONNECTED:
		API_LOG("WiFi", "WiFi STA connected");
		break;
	case SYSTEM_EVENT_WIFI_READY:
		API_LOG("WiFi", "WiFi interface ready");
		break;
	case SYSTEM_EVENT_STA_START:
		API_LOG("WiFi", "WiFi client started");
		break;
	case SYSTEM_EVENT_STA_STOP:
		API_LOG("WiFi", "WiFi clients stopped");
		break;
	case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
		API_LOG("WiFi", "Authentication mode of access point has changed");
		break;
	case SYSTEM_EVENT_STA_LOST_IP:
		API_LOG("WiFi", "Lost IP address and IP address is reset to 0");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
		API_LOG("WiFi", "WiFi Protected Setup (WPS): succeeded in enrollee mode");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_FAILED:
		API_LOG("WiFi", "WiFi Protected Setup (WPS): failed in enrollee mode");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
		API_LOG("WiFi", "WiFi Protected Setup (WPS): timeout in enrollee mode");
		break;
	case SYSTEM_EVENT_STA_WPS_ER_PIN:
		API_LOG("WiFi", "WiFi Protected Setup (WPS): pin code in enrollee mode");
		break;
	default:
		break;
	}
}

/**
 * Create unique device name from MAC address
 **/
void create_dev_name(void)
{
	// Get MAC address for WiFi station
	esp_wifi_get_mac(WIFI_IF_STA, devAddrArray);

	// Write unique name into g_ap_name
	sprintf(g_ap_name, "MHC-SMA-%02X%02X%02X%02X%02X%02X",
			devAddrArray[0], devAddrArray[1],
			devAddrArray[2], devAddrArray[3],
			devAddrArray[4], devAddrArray[5]);
	API_LOG("WiFi", "Device name: %s", g_ap_name);
}

/**
 * Initialize WiFi
 * - Check if WiFi credentials are stored in the preferences
 * - Create unique device name
 * - Register WiFi event callback function
 * - Try to connect to WiFi if credentials are available
 */
void init_wifi(void)
{
	get_wifi_prefs();

	if (!g_has_credentials)
	{
		return;
	}

	WiFi.disconnect(true);
	delay(100);
	WiFi.enableSTA(true);
	delay(100);
	WiFi.mode(WIFI_STA);
	delay(100);
	WiFi.onEvent(wifi_event_cb);

	create_dev_name();

	if (g_has_credentials)
	{
		// Using WiFiMulti to connect to best AP
		wifi_multi.addAP(g_ssid_prim.c_str(), g_pw_prim.c_str());
		wifi_multi.addAP(g_ssid_sec.c_str(), g_pw_sec.c_str());

		wifi_multi.run();
	}
}

#endif // ESP32