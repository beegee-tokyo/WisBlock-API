# WisBlock-API [![Build Status](https://github.com/beegee-tokyo/WisBlock-API/workflows/RAK%20Library%20Build%20CI/badge.svg)](https://github.com/beegee-tokyo/WisBlock-API/actions)

| <center><img src="./assets/rakstar.jpg" alt="RAKstar" width=50%></center>  | <center><img src="./assets/RAK-Whirls.png" alt="RAKWireless" width=50%></center> | <center><img src="./assets/WisBlock.png" alt="WisBlock" width=50%></center> | <center><img src="./assets/Yin_yang-48x48.png" alt="BeeGee" width=50%></center>  |
| -- | -- | -- | -- |

----

Targeting low power consumption, this Arduino library for RAKwireless WisBlock Core modules takes care of all the LoRa P2P, LoRaWAN, BLE, AT command functionality. You can concentrate on your application and leave the rest to the API. It is made as a companion to the [SX126x-Arduino LoRaWAN library](https://github.com/beegee-tokyo/SX126x-Arduino) :arrow_upper_right:    
It requires some rethinking about Arduino applications, because you have no `setup()` or `loop()` function. Instead everything is event driven. The MCU is sleeping until it needs to take actions. This can be a LoRaWAN event, an AT command received over the USB port or an application event, e.g. an interrupt coming from a sensor. 

This approach makes it easy to create applications designed for low power usage. During sleep the WisBlock Base + WisBlock Core RAK4631 consume only 40uA.

In addition the API offers two options to setup LoRa P2P / LoRaWAN settings without the need to hard-code them into the source codes.    
- AT Commands => [AT-Commands Manual](./AT-Commands.md)
- BLE interface to [WisBlock Toolbox](https://play.google.com/store/apps/details?id=tk.giesecke.wisblock_toolbox) ↗️

# _**IMPORTANT: This first release supports only the [RAKwireless WisBlock RAK4631 Core Module](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview)**_ ↗️ _**and the [RAKwireless WisBlock RAK11310 Core Module](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK11310/Overview)**_ ↗️

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
	* [Reset WisBlock Core module](#reset_wisblock_core-module)
	* [Wake up loop](#wake-up-loop)
	* [Print settings to log output](#print-settings-to-log-output)
	* [Stop application wakeup timer](#stop-application-wakeup-timer)
	* [Restart application timer with a new value](#restart-application-timer-with-a-new-value)    
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

The API is handling everything from **`setup()`**, **`loop()`**, LoRaWAN initialization, LoRaWAN events handling, BLE initialization, BLE events handling to the AT command interface.    

_**REMARK!**_    
The user application **MUST NOT** have the functions **`setup()`** and **`loop()`**!    

The user application has two initialization functions, one is called at the beginning of **`setup()`**, the other one at the very end. The other functions are event callbacks that are called from **`loop()`**. It is possible to define custom events (like interrupts from a sensor) as well. 

Sensor reading, actuator control or other application tasks are handled in the **`app_event_handler()`**. **`app_event_handler()`** is called frequently, the time between calls is defined by the application. In addition **`app_event_handler()`** is called on custom events.

**`ble_data_handler()`** is called on BLE events (BLE UART RX events for now) from **`loop()`**. It can be used to implement custom communication over BLE UART.

**REMARK**    
This function is not required on the RAK11310!    

**`lora_data_handler()`** is called on different LoRaWAN events    
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
Starting with WisBlock API V1.1.2 the AT Commands can be extended by user defined AT Commands. This new implementation uses the parser function of the WisBlock API AT command function. In addition, custom AT commands will be listed if the **`AT?`** is used.    
To extend the AT commands, three steps are required:    

### 1) Definition of the custom AT commands
The custom AT commands are listed in an array with the struct atcmd_t format. Each entry consist of the AT command, the explanation text that is shown when the command is called with a ? and pointers to the functions for query, execute with parameters and execute without parameters. Here is an example for two custom AT commands:    
```cpp
atcmd_t g_user_at_cmd_list[] = {
	/*|    CMD    |     AT+CMD?      |    AT+CMD=?    |  AT+CMD=value |  AT+CMD  |*/
	// GNSS commands
	{"+SETVAL", "Get/Set custom variable", at_query_value, at_exec_value, NULL},
	{"+LIST", "Show last packet content", at_query_packet, NULL, NULL},
};
```
**REMARK 1**    
For functions that are not supported by the AT command a **`NULL`** must be put into the array.    
**REMARK 2**    
The name **`g_user_at_cmd_list`** is fixed and cannot be changed or the custom commands are not detected.    

### 2) Definition of the number of custom AT commands
A variable with the number of custom AT commands must be provided:
```cpp
/** Number of user defined AT commands */
uint8_t g_user_at_cmd_num = sizeof(g_user_at_cmd_list) / sizeof(atcmd_t);
```
**REMARK**    
The name **`g_user_at_cmd_num`** is fixed and cannot be changed or the custom commands are not detected.

### 3) The functions for query and execute
For each custom command the query and execute commands must be written. The names of these functions must match 
the function names used in the array of custom AT commands. The execute command receives as a parameter the value of the AT command after the **`=`** of the value. 

Query functions (**`=?`**) do not receive and parameters and must always return with 0. Query functions save the result of the query in the global char array **`g_at_query_buffer`**, the array has a max size of **`ATQUERY_SIZE`** which is 128 characters.

Execute functions with parameters (**`=<value>`**) receive values or settings as a pointer to an char array. This array includes only the value or parameter without the AT command itself. For example the execute function handling **`AT+SETDEV=12000`** would receive only the **`120000`**. The received value or parameter must be checked for validity and if the value of format is not matching, an **`AT_ERRNO_PARA_VAL`** must be returned. If the value or parameter is correct, the function should return **`0`**.

Execute functions without parameters are used to perform an action and return the success of the action as either **`0`** if successfull or  **`AT_ERRNO_EXEC_FAIL`** if the execution failed.

### Example
This examples is used to set a variable in the application.

```c++
/*********************************************************************/
// Example AT command to change the value of the variable new_val:
// Query the value AT+SETVAL=?
// Set the value AT+SETVAL=120000
// Second AT command to show last packet content
// Query with AT+LIST=?
/*********************************************************************/
int32_t new_val = 3000;

/**
 * @brief Returns the current value of the custom variable
 * 
 * @return int always 0
 */
static int at_query_value()
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "Custom Value: %d", new_val);
	return 0;
}

/**
 * @brief Command to set the custom variable
 * 
 * @param str the new value for the variable without the AT part
 * @return int 0 if the command was succesfull, 5 if the parameter was wrong
 */
static int at_exec_value(char *str)
{
	new_val = strtol(str, NULL, 0);
	MYLOG("APP", "Value number >>%ld<<", new_val);
	return 0;
}

/**
 * @brief Example how to show the last LoRa packet content
 * 
 * @return int always 0
 */
static int at_query_packet()
{
	snprintf(g_at_query_buf, ATQUERY_SIZE, "Packet: %02X%02X%02X%02X",
			 g_lpwan_data.data_flag1,
			 g_lpwan_data.data_flag2,
			 g_lpwan_data.batt_1,
			 g_lpwan_data.batt_2);
	return 0;
}

/**
 * @brief List of all available commands with short help and pointer to functions
 * 
 */
atcmd_t g_user_at_cmd_list[] = {
	/*|    CMD    |     AT+CMD?      |    AT+CMD=?    |  AT+CMD=value |  AT+CMD  |*/
	// GNSS commands
	{"+SETVAL", "Get/Set custom variable", at_query_value, at_exec_value, NULL},
	{"+LIST", "Show last packet content", at_query_packet, NULL, NULL},
};

/** Number of user defined AT commands */
uint8_t g_user_at_cmd_num = sizeof(g_user_at_cmd_list) / sizeof(atcmd_t);
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
**`void api_set_version(uint16_t sw_1 = 1, uint16_t sw_2 = 0, uint16_t sw_3 = 0);`**    
This function can be called to set the application version. The application version can be requested by AT commands.
The version number is build from three digits:    
**`sw_1`** ==> major version increase on API change / not backwards compatible    
**`sw_2`** ==> minor version increase on API change / backward compatible    
**`sw_3`** ==> patch version increase on bugfix, no affect on API     
If **`api_set_version`** is not called, the application version defaults to **`1.0.0`**.    

----

## Reset WisBlock Core module
**`void api_reset(void);`**    
Performs a reset of the WisBlock Core module

----

## Wake up loop
**`void api_wake_loop(uint16_t reason);`**    
This is used to wakeup the loop with an event. The **`reason`** must be defined in **`app.h`**. After the loop woke app, it will call the **`app_event_handler()`** with the value of **`reason`** in **`g_task_event_type`**.

As an example, this can be used to wakeup the device from the interrupt of an accelerometer sensor. Here as an example an extract from the **`accelerometer`** example code.

In **`accelerometer.ino`** the event is defined. The first define is to set the signal, the second is to clear the event after it was handled.
```c++
/** Define additional events */
#define ACC_TRIGGER 0b1000000000000000
#define N_ACC_TRIGGER 0b0111111111111111
```
Then in **`lis3dh_acc.ino`** in the interrupt callback function **`void acc_int_handler(void)`** the loop is woke up with the signal **`ACC_TRIGGER`**
```c++
void acc_int_handler(void)
{
	// Wake up the task to handle it
	api_wake_loop(ACC_TRIGGER);
}
```
And finally in **`accelerometer.ino`** the event is handled in **`app_event_handler()`**
```c++
	// ACC triggered event
	if ((g_task_event_type & ACC_TRIGGER) == ACC_TRIGGER)
	{
		g_task_event_type &= N_ACC_TRIGGER;
		MYLOG("APP", "ACC IRQ wakeup");
		// Reset ACC IRQ register
		get_acc_int();

		// Set Status flag, it will trigger sending a packet
		g_task_event_type = STATUS;
	}
```

----

## Print settings to log output
**`void api_log_settings(void);`**    
This function can be called to list the complete settings of the WisBlock device over USB. The output looks like:
```txt
Device status:
   RAK11310
   Auto join enabled
   Mode LPWAN
   Network joined
   Send Frequency 120
LPWAN status:
   Dev EUI AC1F09FFFE0142C8
   App EUI 70B3D57ED00201E1
   App Key 2B84E0B09B68E5CB42176FE753DCEE79
   Dev Addr 26021FB4
   NWS Key 323D155A000DF335307A16DA0C9DF53F
   Apps Key 3F6A66459D5EDCA63CBC4619CD61A11E
   OTAA enabled
   ADR disabled
   Public Network
   Dutycycle disabled
   Join trials 30
   TX Power 0
   DR 3
   Class 0
   Subband 1
   Fport 2
   Unconfirmed Message
   Region AS923-3
LoRa P2P status:
   P2P frequency 916000000
   P2P TX Power 22
   P2P BW 125
   P2P SF 7
   P2P CR 1
   P2P Preamble length 8
   P2P Symbol Timeout 0
```

----

## Stop application wakeup timer
**`void api_timer_stop(void)`**    
Stops the timer that wakes up the MCU frequently.

----

## Restart application timer with a new value
**`void api_timer_restart(uint32_t new_time)`**    
Restarts the timer with a new value. The value is in milliseconds

----

## Set hardcoded LoRa P2P settings
**`void api_read_credentials(void);`**    
**`void api_set_credentials(void);`**
If LoRa P2P settings need to be hardcoded (e.g. the frequency, bandwidth, ...) this can be done in **`setup_app()`**.
First the saved settings must be read from flash with **`api_read_credentials();`**, then settings can be changed. After changing the settings must be saved with **`api_set_credentials()`**.
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
Hard coded settings must be set in **`void setup_app(void)`**!

_**REMARK 2**_    
Keep in mind that parameters that are changed from with this method can be changed over AT command or BLE _**BUT WILL BE RESET AFTER A REBOOT**_!

## Send data over BLE UART
**`g_ble_uart.print()`** can be used to send data over the BLE UART. **`print`**, **`println`** and **`printf`** is supported.     

**REMARK**    
This command is not available on the RAK11310!    

----

## Restart BLE advertising
By default the BLE advertising is only active for 30 seconds after power-up/reset to lower the power consumption. By calling **`void restart_advertising(uint16_t timeout);`** the advertising can be restarted for **`timeout`** seconds.

**REMARK**    
This command is not available on the RAK11310!    

----

## Send data over LoRaWAN
**`lmh_error_status send_lora_packet(uint8_t *data, uint8_t size, uint8_t fport = 0);`** is used to send a data packet to the LoRaWAN server. **`*data`** is a pointer to the buffer containing the data, **`size`** is the size of the packet. If the fport is 0, the fPortdefined in the g_lorawan_settings structure is used.

----

## Send data over LoRa P2P
**`bool send_p2p_packet(uint8_t *data, uint8_t size);`** is used to send a data packet Over LORa P2P. **`*data`** is a pointer to the buffer containing the data, **`size`** is the size of the packet.

----

## Check result of LoRaWAN transmission
After the TX cycle (including RX1 and RX2 windows) are finished, the result is hold in the global flag **`g_rx_fin_result`**, the event **`LORA_TX_FIN`** is triggered and the **`lora_data_handler()`** callback is called. In this callback the result can be checked and if necessary measures can be taken.

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

#ifdef NRF52_SERIES
#if MY_DEBUG > 0
#define MYLOG(tag, ...)                     \
	do                                      \
	{                                       \
		if (tag)                            \
			PRINTF("[%s] ", tag);           \
		PRINTF(__VA_ARGS__);                \
		PRINTF("\n");                       \
		if (g_ble_uart_is_connected)        \
		{                                   \
			g_ble_uart.printf(__VA_ARGS__); \
			g_ble_uart.printf("\n");        \
		}                                   \
	} while (0)
#else
#define MYLOG(...)
#endif
#endif
#ifdef ARDUINO_ARCH_RP2040
#if MY_DEBUG > 0
#define MYLOG(tag, ...)                  \
	do                                   \
	{                                    \
		if (tag)                         \
			Serial.printf("[%s] ", tag); \
		Serial.printf(__VA_ARGS__);      \
		Serial.printf("\n");             \
	} while (0)
#else
#define MYLOG(...)
#endif
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

/** Send Fail counter **/
uint8_t send_fail = 0;
```

----

## setup_app
This function is called at the very beginning of the application start. In this function everything should be setup that is required before Arduino **`setup()`** is executed. This could be for example the LoRaWAN credentials.
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
      digitalWrite(LED_GREEN, !digitalRead(LED_GREEN));
    }
    else
    {
      break;
    }
  }
  digitalWrite(LED_GREEN, LOW);
  
  MYLOG("APP", "Setup WisBlock API Example");

#ifdef NRF52_SERIES
  // Enable BLE
  g_enable_ble = true;
#endif

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

#ifdef NRF52_SERIES
    // If BLE is enabled, restart Advertising
    if (g_enable_ble)
    {
      restart_advertising(15);
    }
#endif

    if (lora_busy)
    {
      MYLOG("APP", "LoRaWAN TX cycle not finished, skip this event");
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
          break;
        case LMH_BUSY:
          MYLOG("APP", "LoRa transceiver is busy");
          break;
        case LMH_ERROR:
          MYLOG("APP", "Packet error, too big to send with current DR");
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
BLE communication is only supported on the RAK4631. The RAK11310 does not have BLE.

```c++
#ifdef NRF52_SERIES
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
#endif
```

----

## lora_data_handler
This callback is called on three different events:

### 1 LORA_DATA
The event **LORA_DATA** is triggered if a downlink packet from the LoRaWAN server or a LoRa P2P packet has arrived. In this example we are not parsing the data, they are only printed out to the LOG and over BLE UART (if a device is connected)

### 2 LORA_TX_FIN
The event **LORA_TX_FIN** is triggered after sending an uplink packet is finished, including the RX1 and RX2 windows. If **CONFIRMED** packets are sent, the global flag **`g_rx_fin_result`** contains the result of the confirmed transmission. If **`g_rx_fin_result`** is true, the LoRaWAN server acknowledged the uplink packet by sending an **`ACK`**. Otherwise the **`g_rx_fin_result`** is set to false, indicating that the packet was not received by the LoRaWAN server (no gateway in range, packet got damaged on the air. If **UNCONFIRMED** packets are sent or if **LoRa P2P mode** is used, the flag **`g_rx_fin_result`** is always true. 

### 3 LORA_JOIN_FIN
The event **LORA_JOIN_FIN** is called after the Join request/Join accept/reject cycle is finished. The global flag **`g_task_event_type`** contains the result of the Join request. If true, the node has joined the network. If false the join didn't succeed. In this case the join cycle could be restarted or the node could report an error.

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
  }

  // LoRa TX finished handling
  if ((g_task_event_type & LORA_TX_FIN) == LORA_TX_FIN)
  {
    g_task_event_type &= N_LORA_TX_FIN;

    MYLOG("APP", "LPWAN TX cycle %s", g_rx_fin_result ? "finished ACK" : "failed NAK");

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

_**REMARK**_    
RAK11310 has no BLE and the blue LED can be used for other purposes.    

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
- 2022-01-30
  - Add more API functions and update README
- 2022-01-25
  - Add experimental support for RAK11310
- 2022-01-22
  - Make it easier to use custom AT commands and list them with AT?
- 2022-01-03
  - Instead of endless loop, leave error handling to the application
- 2021-12-06
  - Fix LoRa P2P
- 2021-12-04
  - Fix BLE MTU size
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
