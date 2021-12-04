# SX126x-API
----
Arduino library that takes all the LoRaWAN, BLE, AT command stuff off your workload. You concentrate on your application and leave the rest to the API. It is made as a companion to the [SX126x-Arduino LoRaWAN library](https://github.com/beegee-tokyo/SX126x-Arduino)    

# Release Notes

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
