# WisBlock-API [![Build Status](https://github.com/beegee-tokyo/WisBlock-API/workflows/RAK%20Library%20Build%20CI/badge.svg)](https://github.com/beegee-tokyo/WisBlock-API/actions)

| <center><img src="./assets/rakstar.jpg" alt="RAKstar" width=50%></center>  | <center><img src="./assets/RAK-Whirls.png" alt="RAKWireless" width=50%></center> | <center><img src="./assets/WisBlock.png" alt="WisBlock" width=50%></center> | <center><img src="./assets/Yin_yang-48x48.png" alt="BeeGee" width=50%></center>  |
| -- | -- | -- | -- |

----

Targeting low power consumption, this Arduino library for RAKwireless WisBlock Core modules takes care of all the LoRa P2P, LoRaWAN, BLE, AT command functionality. You can concentrate on your application and leave the rest to the API. It is made as a companion to the [SX126x-Arduino LoRaWAN library](https://github.com/beegee-tokyo/SX126x-Arduino) :arrow_upper_right:    
It requires some rethinking about Arduino applications, because you have no `setup()` or `loop()` function. Instead everything is event driven. The MCU is sleeping until it needs to take actions. This can be a LoRaWAN event, an AT command received over the USB port or an application event, e.g. an interrupt coming from a sensor. 

This approach makes it easy to create applications designed for low power usage. During sleep the WisBlock Base + WisBlock Core RAK4631 consume only 40uA.

In addition the API offers two options to setup LoRa P2P / LoRaWAN settings without the need to hard-code them into the source codes.    
- AT Commands => [AT-Commands Manual](./AT-Commands.md)
- BLE interface to [WisBlock Toolbox](https://play.google.com/store/apps/details?id=tk.giesecke.wisblock_toolbox) :arrow_upper_right:

# _**IMPORTANT: This first release supports only the [RAKwireless WisBlock RAK4631 Core Module](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview)**_ :arrow_upper_right:

----

# Content
* [How does it work?](#how-does-it-work)
	* [Block diagram of the API and application part](#block-diagram-of-the-api-and-application-part)
	* [AT Command format](#at-command-format)
	* [Extend AT command interface](#extend-at-command-interface)
	* [Available examples](#available-examples)
* [API functions](#api-functions)
	* [Set the application version](#set-the-application-version)
	* [Set hardcoded LoRaWAN credentials](#set-hardcoded-lorawan-credentials)
	* [Send data over BLE UART](#send-data-over-ble-uart)
	* [Restart BLE advertising](#restart-ble-advertising)
	* [Send data over LoRaWAN](#send-data-over-lorawan)
	* [Check result of LoRaWAN transmission](#check-result-of-lorawan-transmission)
	* [Trigger custom events](#trigger-custom-events)
		* [Event trigger definition](#event-trigger-definition)
		* [Example for a custom event using the signal of a PIR sensor to wake up the device](#example-for-a-custom-event-using-the-signal-of-a-pir-sensor-to-wake-up-the-device)
* [Simple code example of the user application](#simple-code-example-of-the-user-application)
	* [Includes and definitions](#includes-and-definitions)
	* [setup_app()](#setup-app)
	* [init_app()](#init-app)
	* [app_event_handler()](#app-event-handler)
	* [ble_data_handler()](#ble-data-handler)
	* [lora_data_handler()](#lora-data-handler)
		* [1 LORA_DATA](#1-lora-data)
		* [2 LORA_TX_FIN](#2-lora-tx-fin)
		* [3 LORA_JOIN_FIN](#3-lora-join-fin)
* [Debug and Powersave Settings](#debug-and-powersave-settings)
* [License](#license)
* [Changelog](#changelog)

----

# How does it work?

The API is handling everything from `setup()`, `loop()`, LoRaWAN initialization, LoRaWAN events handling, BLE initialization, BLE events handling to the AT command interface.    

_**REMARK!**_    
The user application **MUST NOT** have the functions `setup()` and `loop()`!    

The user application has two initialization functions, one is called at the beginning of `setup()`, the other one at the very end. The other functions are event callbacks that are called from `loop()`. It is possible to define custom events (like interrupts from a sensor) as well. 

Sensor reading, actuator control or other application tasks are handled in the `app_event_handler()`. `app_event_handler()` is called frequently, the time between calls is defined by the application. In addition `app_event_handler()` is called on custom events.

`ble_data_handler()` is called on BLE events (BLE UART RX events for now) from `loop()`. It can be used to implement custom communication over BLE UART.

`lora_data_handler()` is called on different LoRaWAN events    
- A download packet has arrived from the LoRaWAN server
- A LoRaWAN transmission has been finished
- Join Accept or Join Failed

----

## Block diagram of the API and application part
![Block diagram](./assets/Structure.png)    

----

## AT Command format
All available AT commands can be found in the [AT-Commands Manual](./AT-Commands.md)

----

## Extend AT command interface
Starting with WisBlock API V1.1.2 the AT Commands can be extended by user defined AT Commands. Here is an example code how to add a user AT command. To extend the AT commands, a function **`user_at_handler`** has to be written.
This function will be called by the AT command handler if the AT command is not recognied. 
**`user_at_handler**` has two inputs:
- `char * user_cmd` a pointer to an char array with the AT command
- `uint8_t cmd_size` is the length of the AT command char array.

This examples is used to set a variable in the application.

```c++
/*********************************************************************/
// Example AT command to change the value of the variable new_val:
// Query the value AT+SETVAL=?
// Set the value AT+SETVAL=120000
/*********************************************************************/
int32_t new_val = 3000;

bool user_at_handler(char *user_cmd, uint8_t cmd_size)
{
	MYLOG("APP", "Received User AT commmand >>%s<< len %d", user_cmd, cmd_size);

	// Get the command itself
	char *param;

	param = strtok(user_cmd, "=");
	MYLOG("APP", "Commmand >>%s<<", param);

	// Check if the command is supported
	if (strcmp(param, (const char *)"+SETVAL") == 0)
	{
		// check if it is query or set
		param = strtok(NULL, ":");
		MYLOG("APP", "Param string >>%s<<", param);

		if (strcmp(param, (const char *)"?") == 0)
		{
			// It is a query, use AT_PRINTF to respond
			AT_PRINTF("NEW_VAL: %d", new_val);
		}
		else
		{
			new_val = strtol(param, NULL, 0);
			MYLOG("APP", "Value number >>%ld<<", new_val);
		}
		return true;
	}
	return false;
}
```

----

## Available examples
- [API Test](./examples/api-test) is a very basic example that sends a dummy message over LoRaWAN
- [Environment Sensor](./examples/environment) shows how to use the frequent wake up call to read sensor data from a RAK1906
- [Accelerometer Sensor](./examples/accel) shows how to use an external interrupt to create a wake-up event.
- [WisBlock Kit 2 GNSS tracker](./examples/WisBlock-Kit-2) is a LPWAN GNSS tracker application for the [WisBlock Kit2](https://store.rakwireless.com/collections/kits-bundles/products/wisblock-kit-2-lora-based-gps-tracker-with-solar-panel) :arrow_upper_right:    
- [LoRa P2P](./examples/LoRa-P2P) is a simple LoRa P2P example using the WisBlock API    

These five examples explain the usage of the API. In all examples the API callbacks and the additional functions (sensor readings, IRQ handling, GNSS location service) are separated into their own sketches.    
- The simplest example (_**api-test.ino**_) just sends a 3 byte packet with the values 0x10, 0x00, 0x00.    
- The other examples send their data encoded in the same format as RAKwireless WisNode devices. An explanation of the data format can be found in the RAKwireless [Documentation Center](https://docs.rakwireless.com/Product-Categories/WisTrio/RAK7205-5205/Quickstart/#decoding-sensor-data-on-chirpstack-and-ttn) :arrow_upper_right:. A ready to use packet decoder can be found in the RAKwireless Github repos in [RUI_LoRa_node_payload_decoder](https://github.com/RAKWireless/RUI_LoRa_node_payload_decoder) :arrow_upper_right:
- The LoRa P2P example (_**LoRa-P2P.ino**_) listens for P2P packets and displays them in the log.    

_**The WisBlock-API has been used as well in the following PlatformIO projects:**_
- _**[RAK4631-Kit-4-RAK1906](https://github.com/beegee-tokyo/RAK4631-Kit-4-RAK1906) :arrow_upper_right: Environment sensor application for the [WisBlock Kit 4](https://store.rakwireless.com/collections/kits-bundles/products/wisblock-kit-4-air-quality-monitor)**_ :arrow_upper_right:
- _**[RAK4631-Kit-2-RAK1910-RAK1904-RAK1906](https://github.com/beegee-tokyo/RAK4631-Kit-2-RAK1910-RAK1904-RAK1906) :arrow_upper_right: LPWAN GNSS tracker application for the [WisBlock Kit 2](https://store.rakwireless.com/collections/kits-bundles/products/wisblock-kit-2-lora-based-gps-tracker-with-solar-panel)**_ :arrow_upper_right:
- _**[RAK4631-Kit-2-RAK12500-RAK1906](https://github.com/beegee-tokyo/RAK4631-Kit-2-RAK12500-RAK1906) :arrow_upper_right: LPWAN GNSS tracker application using the [RAK12500](https://store.rakwireless.com/products/wisblock-gnss-location-module-rak12500)**_ :arrow_upper_right:

----

# API functions
The API provides some calls for management, to send LoRaWAN packet, to send BLE UART data and to trigger events.

----
## Set the application version
`void api_set_version(uint16_t sw_1 = 1, uint16_t sw_2 = 0, uint16_t sw_3 = 0);`
This function can be called to set the application version. The application version can be requested by AT commands.
The version number is build from three digits:    
`sw_1` ==> major version increase on API change / not backwards compatible    
`sw_2` ==> minor version increase on API change / backward compatible    
`sw_3` ==> patch version increase on bugfix, no affect on API     
If `api_set_version` is not called, the application version defaults to **`1.0.0`**.    

----

## Set hardcoded LoRaWAN credentials
`void api_read_credentials(void);`    
`void api_set_credentials(void);`    
If LoRaWAN credentials need to be hardcoded (e.g. the region, the send repeat time, ...) this can be done in `setup_app()`.
First the saved credentials must be read from flash with `api_read_credentials();`, then credentials can be changed. After changing the credentials must be saved with `api_set_credentials()`.
As the WisBlock API checks if any changes need to be saved, the changed values will be only saved on the first boot after flashing the application.     
Example:    
```c++
// Read credentials from Flash
api_read_credentials();
// Make changes to the credentials
g_lorawan_settings.send_repeat_time = 240000;                   // Default is 2 minutes
g_lorawan_settings.subband_channels = 2;                        // Default is subband 1
g_lorawan_settings.app_port = 4;                                // Default is 2
g_lorawan_settings.confirmed_msg_enabled = LMH_CONFIRMED_MSG;   // Default is UNCONFIRMED
g_lorawan_settings.lora_region = LORAMAC_REGION_EU868;          // Default is AS923-3
// Save hard coded LoRaWAN settings
api_set_credentials();
```

_**REMARK 1**_    
Hard coded credentials must be set in `void setup_app(void)`!

_**REMARK 2**_    
Keep in mind that parameters that are changed from with this method can be changed over AT command or BLE _**BUT WILL BE RESET AFTER A REBOOT**_!

----

## Set hardcoded LoRa P2P settings
`void api_read_credentials(void);`    
`void api_set_credentials(void);`
If LoRa P2P settings need to be hardcoded (e.g. the frequency, bandwidth, ...) this can be done in `setup_app()`.
First the saved settings must be read from flash with `api_read_credentials();`, then settings can be changed. After changing the settings must be saved with `api_set_credentials()`.
As the WisBlock API checks if any changes need to be saved, the changed values will be only saved on the first boot after flashing the application.     
Example:    
```c++
// Read credentials from Flash
api_read_credentials();

// Make changes to the credentials
g_lorawan_settings.p2p_frequency = 916000000;			// Use 916 MHz to send and receive
g_lorawan_settings.p2p_bandwidth = 0;					// Bandwidth 125 kHz
g_lorawan_settings.p2p_sf = 7;							// Spreading Factor 7
g_lorawan_settings.p2p_cr = 1;							// Coding Rate 4/5
g_lorawan_settings.p2p_preamble_len = 8;				// Preample Length 8
g_lorawan_settings.p2p_tx_power = 22;					// TX power 22 dBi

// Save hard coded LoRaWAN settings
api_set_credentials();
```

_**REMARK 1**_    
Hard coded settings must be set in `void setup_app(void)`!

_**REMARK 2**_    
Keep in mind that parameters that are changed from with this method can be changed over AT command or BLE _**BUT WILL BE RESET AFTER A REBOOT**_!

## Send data over BLE UART
`g_ble_uart.print()` can be used to send data over the BLE UART. `print`, `println` and `printf` is supported.

----

## Restart BLE advertising
By default the BLE advertising is only active for 30 seconds after power-up/reset to lower the power consumption. By calling `void restart_advertising(uint16_t timeout);` the advertising can be restarted for `timeout` seconds.

----

## Send data over LoRaWAN
`lmh_error_status send_lora_packet(uint8_t *data, uint8_t size, uint8_t fport = 0);` is used to send a data packet to the LoRaWAN server. `*data` is a pointer to the buffer containing the data, `size` is the size of the packet. If the fport is 0, the fPortdefined in the g_lorawan_settings structure is used.

----

## Send data over LoRa P2P
`bool send_p2p_packet(uint8_t *data, uint8_t size);` is used to send a data packet Over LORa P2P. `*data` is a pointer to the buffer containing the data, `size` is the size of the packet.

----

## Check result of LoRaWAN transmission
After the TX cycle (including RX1 and RX2 windows) are finished, the result is hold in the global flag `g_rx_fin_result`, the event `LORA_TX_FIN` is triggered and the `lora_data_handler()` callback is called. In this callback the result can be checked and if necessary measures can be taken.

----

## Trigger custom events
Beside of the pre-defined wake-up events of the API, additional wake_up events can be defined. These events must be handled in the `app_event_handler()` callback.

----

### Event trigger definition
An event trigger is split into two parts
- `g_task_event_type` is holding a flag for the event.
- `g_task_sem` is a semphore that is used to wake up the `loop()` and handle the event.    

`g_task_event_type` is a 16 bit variable where each single bit represents a different event. The lower 7 bits are used for API events, the upper 9 bits can be used for custom application events.

----

### Example for a custom event using the signal of a PIR sensor to wake up the device
```c++
// Define the event flag and the clear flag for the event
#define PIR_TRIGGER   0b1000000000000000
#define N_PIR_TRIGGER 0b0111111111111111

// This handler is called when the PIR trigger goes high
// It sets the PIR_TRIGGER event and wakes up the loop()
void pir_irq_handler(void)
{
	// Wake up task to handle PIR trigger
	g_task_event_type |= PIR_TRIGGER;
	// Notify task about the event
	if (g_task_sem != NULL)
	{
		MYLOG("PIR", "PIR_TRIGGERED");
		xSemaphoreGive(g_task_sem);
	}
}

// PIR function setup in the app_setup() function
void app_setup()
{
	...
	// Set input pin for PIR trigger impulse
	pinMode(WB_IO5, INPUT_PULLDOWN);
	attachInterrupt(WB_IO5,pir_trigger_handler, RISING);
	...
}

// Handling of the PIR event in the app_event_handler() function
void app_event_handler(void)
{
	...
	// PIR triggered event
	if ((g_task_event_type & PIR_TRIGGER) == PIR_TRIGGER)
	{
    	g_task_event_type &= N_PIR_TRIGGER;
		/// \todo do something when the PIR is triggered, e.g switch on a light over a relay
	}
}
```

----

# Simple code example of the user application
The code used here is the [api-test.ino](./examples/api-test) example.

----

## Includes and definitions
These are the required includes and definitions for the user application and the API interface    
In this example we hard-coded the LoRaWAN credentials. It is strongly recommended **TO NOT DO THAT** to avoid duplicated node credentials    
Alternative options to setup credentials are
- over USB with [AT-Commands](./AT-Commands.md)
- over BLE with [WisBlock Toolbox](https://play.google.com/store/apps/details?id=tk.giesecke.wisblock_toolbox) :arrow_upper_right: 

```c++
#include <Arduino.h>
/** Add you required includes after Arduino.h */
#include <Wire.h>

// Debug output set to 0 to disable app debug output
#ifndef MY_DEBUG
#define MY_DEBUG 1
#endif

#if MY_DEBUG > 0
#define MYLOG(tag, ...)           \
  do                            \
  {                             \
    if (tag)                  \
      PRINTF("[%s] ", tag); \
    PRINTF(__VA_ARGS__);      \
    PRINTF("\n");             \
  } while (0)
#else
#define MYLOG(...)
#endif

/** Include the WisBlock-API */
#include <WisBlock-API.h> // Click to install library: http://librarymanager/All#WisBlock-API

/** Define the version of your SW */
#define SW_VERSION_1 1 // major version increase on API change / not backwards compatible
#define SW_VERSION_2 0 // minor version increase on API change / backward compatible
#define SW_VERSION_3 0 // patch version increase on bugfix, no affect on API

/**
   Optional hard-coded LoRaWAN credentials for OTAA and ABP.
   It is strongly recommended to avoid duplicated node credentials
   Options to setup credentials are
   - over USB with AT commands
   - over BLE with My nRF52 Toolbox
*/
uint8_t node_device_eui[8] = {0x00, 0x0D, 0x75, 0xE6, 0x56, 0x4D, 0xC1, 0xF3};
uint8_t node_app_eui[8] = {0x70, 0xB3, 0xD5, 0x7E, 0xD0, 0x02, 0x01, 0xE1};
uint8_t node_app_key[16] = {0x2B, 0x84, 0xE0, 0xB0, 0x9B, 0x68, 0xE5, 0xCB, 0x42, 0x17, 0x6F, 0xE7, 0x53, 0xDC, 0xEE, 0x79};
uint8_t node_nws_key[16] = {0x32, 0x3D, 0x15, 0x5A, 0x00, 0x0D, 0xF3, 0x35, 0x30, 0x7A, 0x16, 0xDA, 0x0C, 0x9D, 0xF5, 0x3F};
uint8_t node_apps_key[16] = {0x3F, 0x6A, 0x66, 0x45, 0x9D, 0x5E, 0xDC, 0xA6, 0x3C, 0xBC, 0x46, 0x19, 0xCD, 0x61, 0xA1, 0x1E};
```

Forward declarations of some functions (required when using PlatformIO)    
```c++
/** Application function definitions */
void setup_app(void);
bool init_app(void);
void app_event_handler(void);
void ble_data_handler(void) __attribute__((weak));
void lora_data_handler(void);
```

Here the application name is set to **RAK-TEST**. The name will be extended with the nRF52 unique chip ID. This name is used in the BLE advertising.
```c++
/** Application stuff */

/** Set the device name, max length is 10 characters */
char g_ble_dev_name[10] = "RAK-TEST";
```

Some flags and signals required
```c++
/** Flag showing if TX cycle is ongoing */
bool lora_busy = false;

/** Required to give semaphore from ISR. Giving the semaphore wakes up the loop() */
BaseType_t g_higher_priority_task_woken = pdTRUE;

/** Send Fail counter **/
uint8_t send_fail = 0;
```

----

## setup_app
This function is called at the very beginning of the application start. In this function everything should be setup that is required before Arduino `setup()` is executed. This could be for example the LoRaWAN credentials.
In this example we hard-coded the LoRaWAN credentials. It is strongly recommended **TO NOT DO THAT** to avoid duplicated node credentials    
Alternative options to setup credentials are
- over USB with [AT-Commands](./AT-Commands.md)
- over BLE with [WisBlock Toolbox](https://play.google.com/store/apps/details?id=tk.giesecke.wisblock_toolbox) :arrow_upper_right: 
In this function the global flag `g_enable_ble` is set. If true, the BLE interface is initialized. If false, the BLE interface is not activated, which can lower the power consumption.
```c++
void setup_app(void)
{
  Serial.begin(115200);
  time_t serial_timeout = millis();
  // On nRF52840 the USB serial is not available immediately
  while (!Serial)
  {
    if ((millis() - serial_timeout) < 5000)
    {
      delay(100);
      digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
    else
    {
      break;
    }
  }
  digitalWrite(LED_BUILTIN, LOW);
  
  MYLOG("APP", "Setup WisBlock API Example");

  // Enable BLE
  g_enable_ble = true;

  // Set firmware version
  api_set_version(SW_VERSION_1, SW_VERSION_2, SW_VERSION_3);

  // Optional
  // Setup LoRaWAN credentials hard coded
  // It is strongly recommended to avoid duplicated node credentials
  // Options to setup credentials are
  // -over USB with AT commands
  // -over BLE with My nRF52 Toolbox

  // Read LoRaWAN settings from flash
  api_read_credentials();
  // Change LoRaWAN settings
  g_lorawan_settings.auto_join = true;							// Flag if node joins automatically after reboot
  g_lorawan_settings.otaa_enabled = true;							// Flag for OTAA or ABP
  memcpy(g_lorawan_settings.node_device_eui, node_device_eui, 8); // OTAA Device EUI MSB
  memcpy(g_lorawan_settings.node_app_eui, node_app_eui, 8);		// OTAA Application EUI MSB
  memcpy(g_lorawan_settings.node_app_key, node_app_key, 16);		// OTAA Application Key MSB
  memcpy(g_lorawan_settings.node_nws_key, node_nws_key, 16);		// ABP Network Session Key MSB
  memcpy(g_lorawan_settings.node_apps_key, node_apps_key, 16);	// ABP Application Session key MSB
  g_lorawan_settings.node_dev_addr = 0x26021FB4;					// ABP Device Address MSB
  g_lorawan_settings.send_repeat_time = 120000;					// Send repeat time in milliseconds: 2 * 60 * 1000 => 2 minutes
  g_lorawan_settings.adr_enabled = false;							// Flag for ADR on or off
  g_lorawan_settings.public_network = true;						// Flag for public or private network
  g_lorawan_settings.duty_cycle_enabled = false;					// Flag to enable duty cycle (validity depends on Region)
  g_lorawan_settings.join_trials = 5;								// Number of join retries
  g_lorawan_settings.tx_power = 0;								// TX power 0 .. 15 (validity depends on Region)
  g_lorawan_settings.data_rate = 3;								// Data rate 0 .. 15 (validity depends on Region)
  g_lorawan_settings.lora_class = 0;								// LoRaWAN class 0: A, 2: C, 1: B is not supported
  g_lorawan_settings.subband_channels = 1;						// Subband channel selection 1 .. 9
  g_lorawan_settings.app_port = 2;								// Data port to send data
  g_lorawan_settings.confirmed_msg_enabled = LMH_UNCONFIRMED_MSG; // Flag to enable confirmed messages
  g_lorawan_settings.resetRequest = true;							// Command from BLE to reset device
  g_lorawan_settings.lora_region = LORAMAC_REGION_AS923_3;		// LoRa region
  // Save LoRaWAN settings
  api_set_credentials();
```

----

## init_app
This function is called after BLE and LoRa are already initialized. Ideally this is the place to initialize application specific stuff like sensors or actuators. In this example it is unused    

```c++
/**
 * @brief Application specific initializations
 * 
 * @return true Initialization success
 * @return false Initialization failure
 */
bool init_app(void)
{
	MYLOG("APP", "init_app");
	return true;
}
```

----

## app_event_handler
This callback is called on the **STATUS** event. The **STATUS** event is triggered frequently, the time is set by `send_repeat_time`. It is triggered as well by user defined events. See example **RAK1904_example**_ how user defined events are defined.
_**It is important that event flags are reset. As example the STATUS event is reset by this code sequence:**
```c++
if ((g_task_event_type & STATUS) == STATUS)
{
	g_task_event_type &= N_STATUS;
	...
}
```
The **STATUS** event is used to send frequently uplink packets to the LoRaWAN server.    
In this example code we restart as well the BLE advertising for 15 seconds. Otherwise BLE adverstising is only active for 30 seconds after power-up/reset.    
```c++
void app_event_handler(void)
{
  // Timer triggered event
  if ((g_task_event_type & STATUS) == STATUS)
  {
    g_task_event_type &= N_STATUS;
    MYLOG("APP", "Timer wakeup");

    // If BLE is enabled, restart Advertising
    if (g_enable_ble)
    {
      restart_advertising(15);
    }

    if (lora_busy)
    {
      MYLOG("APP", "LoRaWAN TX cycle not finished, skip this event");
      if (g_ble_uart_is_connected)
      {
        g_ble_uart.println("LoRaWAN TX cycle not finished, skip this event");
      }
    }
    else
    {

      // Dummy packet

      uint8_t dummy_packet[] = {0x10, 0x00, 0x00};

      lmh_error_status result = send_lora_packet(dummy_packet, 3);
      switch (result)
      {
        case LMH_SUCCESS:
          MYLOG("APP", "Packet enqueued");
          // Set a flag that TX cycle is running
          lora_busy = true;
          if (g_ble_uart_is_connected)
          {
            g_ble_uart.println("Packet enqueued");
          }
          break;
        case LMH_BUSY:
          MYLOG("APP", "LoRa transceiver is busy");
          if (g_ble_uart_is_connected)
          {
            g_ble_uart.println("LoRa transceiver is busy");
          }
          break;
        case LMH_ERROR:
          MYLOG("APP", "Packet error, too big to send with current DR");
          if (g_ble_uart_is_connected)
          {
            g_ble_uart.println("Packet error, too big to send with current DR");
          }
          break;
      }
    }
  }
}
```

----

## ble_data_handler
This callback is used to handle data received over the BLE UART. If you do not need BLE UART functionality, you can remove this function completely.
In this example we forward the received BLE UART data to the AT command interpreter. This way, we can submit AT commands either over the USB port or over the BLE UART port.    
```c++
void ble_data_handler(void)
{
  if (g_enable_ble)
  {
    /**************************************************************/
    /**************************************************************/
    /// \todo BLE UART data arrived
    /// \todo or forward them to the AT command interpreter
    /// \todo parse them here
    /**************************************************************/
    /**************************************************************/
    if ((g_task_event_type & BLE_DATA) == BLE_DATA)
    {
      MYLOG("AT", "RECEIVED BLE");
      // BLE UART data arrived
      // in this example we forward it to the AT command interpreter
      g_task_event_type &= N_BLE_DATA;

      while (g_ble_uart.available() > 0)
      {
        at_serial_input(uint8_t(g_ble_uart.read()));
        delay(5);
      }
      at_serial_input(uint8_t('\n'));
    }
  }
}
```

----

## lora_data_handler
This callback is called on two different events:

### 1 LORA_DATA
The event **LORA_DATA** is triggered if a downlink packet from the LoRaWAN server or a LoRa P2P packet has arrived. In this example we are not parsing the data, they are only printed out to the LOG and over BLE UART (if a device is connected)

### 2 LORA_TX_FIN
The event **LORA_TX_FIN** is triggered after sending an uplink packet is finished, including the RX1 and RX2 windows. If **CONFIRMED** packets are sent, the global flag `g_rx_fin_result` contains the result of the confirmed transmission. If `g_rx_fin_result` is true, the LoRaWAN server acknowledged the uplink packet by sending an `ACK`. Otherwise the `g_rx_fin_result` is set to false, indicating that the packet was not received by the LoRaWAN server (no gateway in range, packet got damaged on the air. If **UNCONFIRMED** packets are sent or if **LoRa P2P mode** is used, the flag `g_rx_fin_result` is always true. 

### 3 LORA_JOIN_FIN
The event **LORA_JOIN_FIN** is called after the Join request/Join accept/reject cycle is finished. The global flag `g_task_event_type` contains the result of the Join request. If true, the node has joined the network. If false the join didn't succeed. In this case the join cycle could be restarted or the node could report an error.

```c++
void lora_data_handler(void)
{
  // LoRa data handling
  if ((g_task_event_type & LORA_DATA) == LORA_DATA)
  {
    /**************************************************************/
    /**************************************************************/
    /// \todo LoRa data arrived
    /// \todo parse them here
    /**************************************************************/
    /**************************************************************/
    g_task_event_type &= N_LORA_DATA;
    MYLOG("APP", "Received package over LoRa");
    char log_buff[g_rx_data_len * 3] = {0};
    uint8_t log_idx = 0;
    for (int idx = 0; idx < g_rx_data_len; idx++)
    {
      sprintf(&log_buff[log_idx], "%02X ", g_rx_lora_data[idx]);
      log_idx += 3;
    }
    lora_busy = false;
    MYLOG("APP", "%s", log_buff);

    if (g_ble_uart_is_connected && g_enable_ble)
    {
      for (int idx = 0; idx < g_rx_data_len; idx++)
      {
        g_ble_uart.printf("%02X ", g_rx_lora_data[idx]);
      }
      g_ble_uart.println("");
    }
  }

  // LoRa TX finished handling
  if ((g_task_event_type & LORA_TX_FIN) == LORA_TX_FIN)
  {
    g_task_event_type &= N_LORA_TX_FIN;

    MYLOG("APP", "LPWAN TX cycle %s", g_rx_fin_result ? "finished ACK" : "failed NAK");
    if (g_ble_uart_is_connected)
    {
      g_ble_uart.printf("LPWAN TX cycle %s", g_rx_fin_result ? "finished ACK" : "failed NAK");
    }

    if (!g_rx_fin_result)
    {
      // Increase fail send counter
      send_fail++;

      if (send_fail == 10)
      {
        // Too many failed sendings, reset node and try to rejoin
        delay(100);
        sd_nvic_SystemReset();
      }
    }

    // Clear the LoRa TX flag
    lora_busy = false;
  }

  // LoRa Join finished handling
  if ((g_task_event_type & LORA_JOIN_FIN) == LORA_JOIN_FIN)
  {
    g_task_event_type &= N_LORA_JOIN_FIN;
    if (g_join_result)
    {
      MYLOG("APP", "Successfully joined network");
    }
    else
    {
      MYLOG("APP", "Join network failed");
      /// \todo here join could be restarted.
      // lmh_join();
    }
  }
}
```

----

# Debug and Powersave Settings

## Arduino    
In Arduino it is not possible to define settings in the .ino file that can control behaviour of the the included libraries. To change debug log and usage of the blue BLE LED you have to open the file [**`WisBlock-API.h`**](./src/WisBlock.h) in the libraries source folder.

----
### Debug Log Output
To enable/disable the API debug (**`API_LOG()`**) open the file [**`WisBlock-API.h`**](./src/WisBlock.h) in the libraries source folder.    
Look for 
```c++
#define API_DEBUG 1
```
in the file.      

    0 -> No debug output
    1 -> API debug output

To enable/disable the application debug (**`MY_LOG()`**) You can find in the examples (either in the .ino file or app.h)     
```c++
#define MY_DEBUG 1
```
in the file.      

    0 -> No debug output
    1 -> Application debug output


----

### Disable the blue BLE LED
Look for 
```c++
#define NO_BLE_LED 1
```
in the file **`WisBlock-API`**     

    0 -> the blue LED will be used to indicate BLE status
    1 -> the blue LED will not used

----

## PlatformIO
Debug output can be controlled by defines in the platformio.ini
**`API_DEBUG`** controls debug output of the WisBlock API

    0 -> No debug outpuy
    1 -> WisBlock API debug output

**`MY_DEBUG`** controls debug output of the application itself

    0 -> No debug outpuy
    1 -> Application debug output

**`NO_BLE_LED`** controls the usage of the blue BLE LED.    

    0 -> the blue LED will be used to indicate BLE status
    1 -> the blue LED will not used

Example for no debug output and no blue LED
```ini
build_flags = 
	-DAPI_DEBUG=0    ; 0 Disable WisBlock API debug output
	-DMY_DEBUG=0     ; 0 Disable application debug output
	-DNO_BLE_LED=1   ; 1 Disable blue LED as BLE notificator
```

----
# License    
Library published under MIT license    

**Credits:**    
AT Command functions: Taylor Lee (taylor.lee@rakwireless.com)

----
# Changelog
[Code releases](CHANGELOG.md)
- 2021-11-26
  - Keep it backward compatible, rename LoRa P2P send function
- 2021-11-18
  - Cleanup
- 2021-11-16
  - Update documentation
- 2021-11-15
  - Add LoRa P2P support
  - Add support for user defined AT commands
- 2021-11-06
  - Add library dependency for SX126x-Arduino library
- 2021-11-03
  - Make AT+JOIN conform with RAKwireless AT Command syntax
  - Add AT+STATUS and AT+SEND commands
  - Make AT+NJM compatible with RAK3172
  - Fix compiler warning
  - Fix AT+STATUS command
- 2021-09-29
  - Fixed bug in AT+DR and AT+TXP commands
- 2021-09-25
  - Added `api_read_credentials()`
  - `api_set_credentials()` saves to flash
  - Updated examples
- 2021-09-24:
  - Fix AT command response delay
  - Add AT+MASK command to change channel settings for AU915, EU433 and US915
- 2021-09-19:
  - Change debug output to generic `WisBlock API LoRaWAN` instead of GNSS
- 2021-09-12:
  - Improve examples
- 2021-09-11:
  - V1.0.0 First release
