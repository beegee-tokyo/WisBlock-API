# WisBlock-API
----
Arduino library for RAKWireless WisBlock Core modules that takes all the LoRaWAN, BLE, AT command stuff off your workload. You concentrate on your application and leave the rest to the API. It is made as a companion to the [SX126x-Arduino LoRaWAN library](https://github.com/beegee-tokyo/SX126x-Arduino)    

# Release Notes

## 1.1.16 Small improvements
  - Make AT commands accept lower and upper case
  - AT+BAT=? returns battery level in volt instead of 0 - 254
  - BLE name changed to part of DevEUI for easier recognition of a device
  - Change ADC sampling time to 10 us for better accuracy

## 1.1.15 Remove external NV memory support
  - Removed support for external NV memory. Better to keep this on application level

## 1.1.14 External NV memory support
  - Add support for external NV memory. Prioritize usage of external NV memory over MCU flash memory.
  
## 1.1.13 Fix LoRa P2P bug
  - In LoRa P2P the automatic sending did not work because g_lpwan_has_joined stayed on false.
  
## 1.1.12 Fix timer bug
  - If timer is restarted with a new time, there was a bug that actually stopped the timer
  
## V1.1.11 Fix long sleep problem
  - Define alternate pdMS_TO_TICKS that casts uint64_t for long intervals due to limitation in nrf52840 BSP
  - Switch from using SoftwareTimer for nRF52 to using xTimer due to above problem
  - Change handling of user AT commands for more flexibility

## V1.1.10 P2P bug fix
  - If auto join for LPWAN is disabled, LoRa P2P mode is not working. Fixed.
  - Change response of AT+P2P=? to show bandwidth instead of index number
  - Thanks at @kongduino for finding both problems.

## V1.1.9 Remove sleep limitation
  - Send frequency was limited to 3600 seconds, set it to unlimited
  
## V1.1.8 Add more API functions
  - Add more API functions and update README

## V1.1.7 Add experimental support for RAK11310
  - Move more Core specific functions into the API functions
  - API functions support RAK4631 and RAK11310
  
## V1.1.6 Updated custom AT command handling
  - Make it easier to use custom AT commands and list them with AT?
  
## V1.1.5 Remove endless loop if app_init failed
  - Instead of endless loop, leave error handling to the application
  
## V1.1.4 Fix LoRa P2P
  - Fix bugs in LoRa P2P mode
  
## V1.1.3 Fix BLE MTU size
  - Fix bug with BLE MTU size to support extended LoRaWAN config structure
  
## V1.1.2 Add LoRa P2P support
  - Experimental support for LoRa P2P communication
  - Add support for user defined AT commands
  - Add example for LoRa P2P usage
  
## V1.1.1 Add library dependencies
  - Add SX126x-Arduino library dependency
  
## V1.1.0 Bug fix

## V1.0.9 Fix AT+STATUS command
  - Bugfix
  
## V1.0.8 Fix Compiler warnings
  - Bugfix
  
## V1.0.7 Make AT+NJM compatible with RAK3172/RUI AT commands
  - Adjust AT commands
  
## V1.0.6 Add AT+STATUS and AT+SEND commands
  - Additional AT commands
  
## V1.0.5 Make AT+JOIN conform with RAKwireless AT Command syntax
  - Change separator in AT+JOIN from _**`,`**_ to _**`:`**_
  
## V1.0.4 Fix AT command bugs
  - Fixed bug in AT+DR and AT+TXP commands

## V1.0.3 Add functionality
  - Fix AT command response delay
  - Add AT+MASK command to change channel settings for AU915, EU433 and US915
  - Added `api_read_credentials()`
  - `api_set_credentials()` saves to flash
  - Updated examples

## V1.0.2 Cleanup 
- Change debug output to generic `WisBlock API LoRaWAN` instead of GNSS

## V1.0.1 Improve examples
- Add missing LORA_JOIN_FIN handling in WisBlock-Kit-2 example

## V1.0.0 First release
  - Supports only WisBlock RAK4631 Core Module
