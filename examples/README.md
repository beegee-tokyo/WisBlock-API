# WisBlock API Examples

| <center><img src="../assets/rakstar.jpg" alt="RAKstar" width=50%></center>  | <center><img src="../assets/RAK-Whirls.png" alt="RAKWireless" width=50%></center> | <center><img src="../assets/WisBlock.png" alt="WisBlock" width=50%></center> | <center><img src="../assets/Yin_yang-48x48.png" alt="BeeGee" width=50%></center>  |
| -- | -- | -- | -- |

There are 5 examples to explain the usage of the API. In all examples the API callbacks and the additional functions (sensor readings, IRQ handling, GNSS location service) are separated into their own sketches.    
The simplest example just sends a 3 byte packet with the values 0x10, 0x00, 0x00.    
The other examples send their data encoded in the same format as RAKwireless WisNode devices. An explanation of the data format can be found in the RAKwireless [Documentation Center](https://docs.rakwireless.com/Product-Categories/WisTrio/RAK7205-5205/Quickstart/#decoding-sensor-data-on-chirpstack-and-ttn). A ready to use packet decoder can be found in the RAKwireless Github repos in [RUI_LoRa_node_payload_decoder](https://github.com/RAKWireless/RUI_LoRa_node_payload_decoder)

## Basic Example
[api-test](./api-test)    
This is the basic test app. It is just to show the general structure of the user application. Instead of using AT commands or BLE to setup the LoRaWAN settings, it shows how hard-coded LoRaWAN settings can be used.

### Used hardware
- [RAK5005-O Base Board](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK5005-O/Overview/)
- [RAK4631 Core](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview/)

## Timed Sensor Reading
[environment](./environment)     
This example shows how to use a time triggered event to read environment data from a RAK1906 and send the values over LoRaWAN. The frequency of the timer event can be either set by the BLE interface or with the AT Command **`AT+SENDFREQ`**. Details can be found in the [AT Command Manual](../AT-Commands.md). This example requires to setup the LoRaWAN settings either over the [AT command interface](../AT-Commands.md) or with the Android app [My nRF52 Toolbox](https://play.google.com/store/apps/details?id=tk.giesecke.my_nrf52_tb).

### Used hardware
- [RAK5005-O Base Board](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK5005-O/Overview/)
- [RAK4631 Core](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview/)
- [RAK1906 Environment Sensor](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK1906/Overview/)

## Location Tracker
[WisBlock-Kit-2](./WisBlock-Kit-2)    
This is an example application for the RAKwireless WisBlock Kit 2 (plus an optional RAK1906 environment sensor). This example requires to setup the LoRaWAN settings either over the [AT command interface](../AT-Commands.md) or with the Android app [My nRF52 Toolbox](https://play.google.com/store/apps/details?id=tk.giesecke.my_nrf52_tb).

### Used hardware
- [RAK5005-O Base Board](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK5005-O/Overview/)
- [RAK4631 Core](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview/)
- [RAK1904 Acceleration Sensor](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK1904/Overview/)
- [RAK1910 Location Sensor](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK1910/Overview/)
- [RAK1906 Environment Sensor](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK1906/Overview/)

## LoRa P2P communication
[LoRa P2P](./LoRa-P2P) 
Simple example that shows how to implement LoRa P2P communication.

### Used hardware
- [RAK5005-O Base Board](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK5005-O/Overview/)
- [RAK4631 Core](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview/)

## Interrupt Triggered Event
[accelerometer](./accelerometer)     
This examples shows how to create an interrupt driven event to wake up the MCU and send a data packet over LoRaWAN. This example requires to setup the LoRaWAN settings either over the [AT command interface](../AT-Commands.md) or with the Android app [My nRF52 Toolbox](https://play.google.com/store/apps/details?id=tk.giesecke.my_nrf52_tb).

### Used hardware
- [RAK5005-O Base Board](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK5005-O/Overview/)
- [RAK4631 Core](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK4631/Overview/)
- [RAK1904 Acceleration Sensor](https://docs.rakwireless.com/Product-Categories/WisBlock/RAK1904/Overview/)

In this sketch a custom wake up event is defined
```c++
/** Define additional events */
#define ACC_TRIGGER 0b1000000000000000
#define N_ACC_TRIGGER 0b0111111111111111
```

In the initialization of the accelerometer the sensor is setup to generate an interrupt that is triggered by movement.
```c++
uint8_t dataToWrite = 0;
dataToWrite |= 0x20;                                    //Z high
dataToWrite |= 0x08;                                    //Y high
dataToWrite |= 0x02;                                    //X high
acc_sensor.writeRegister(LIS3DH_INT1_CFG, dataToWrite); // Enable interrupts on high tresholds for x, y and z

dataToWrite = 0;
dataToWrite |= 0x10;                                    // 1/8 range
acc_sensor.writeRegister(LIS3DH_INT1_THS, dataToWrite); // 1/8th range

dataToWrite = 0;
dataToWrite |= 0x01; // 1 * 1/50 s = 20ms
acc_sensor.writeRegister(LIS3DH_INT1_DURATION, dataToWrite);

acc_sensor.readRegister(&dataToWrite, LIS3DH_CTRL_REG5);
dataToWrite &= 0xF3;                                     //Clear bits of interest
dataToWrite |= 0x08;                                     //Latch interrupt (Cleared by reading int1_src)
acc_sensor.writeRegister(LIS3DH_CTRL_REG5, dataToWrite); // Set interrupt to latching

dataToWrite = 0;
dataToWrite |= 0x40; //AOI1 event (Generator 1 interrupt on pin 1)
dataToWrite |= 0x20; //AOI2 event ()
acc_sensor.writeRegister(LIS3DH_CTRL_REG3, dataToWrite);
```

Then the GPIO that is connected to the interrupt line of the acc sensor is setup as interrupt input
```c++
// Set the interrupt callback function
attachInterrupt(WB_IO5, acc_int_handler, RISING);
```

The interrupt callback **`acc_int_handler`** is called when the acc sensor issues an interrupt. In the callback, the event flag **ACC_TRIGGER** is set and by giving a semaphore, the main task is waking up.
```c++
void acc_int_handler(void)
{
	// Set the event flag
	g_task_event_type |= ACC_TRIGGER;
	// Wake up the task to handle it
	xSemaphoreGiveFromISR(g_task_sem, &xHigherPriorityTaskWoken);
}
```

The event **ACC_TRIGGER** is then handled in the **`app_event_handler()`** function. The handler just clears the interrupt registers of the acc sensor and then triggers a new event called **STATUS**. The **STATUS** event triggers the sending of a LoRaWAN packet.
```c++
if ((g_task_event_type & ACC_TRIGGER) == ACC_TRIGGER)
{
	g_task_event_type &= N_STATUS;
	MYLOG("APP", "ACC IRQ wakeup");
	// Reset ACC IRQ register
	get_acc_int();

	// Set Status flag, it will trigger sending a packet
	g_task_event_type = STATUS;
}
```

**`app_event_handler()`** is called a second time, this time with the event **STATUS** which reads the x, y and z acceleration data from the acc chip and sends them as LoRaWAN packet.
```c++
// Timer triggered event
if ((g_task_event_type & STATUS) == STATUS)
{
	g_task_event_type &= N_STATUS;
...
	// Get ACC sensor data
	int16_t acc_val[3] = {0};

	read_acc_vals(acc_val);

	g_acc_data.acc_x_1 = (int8_t)(acc_val[0] >> 8);
	g_acc_data.acc_x_2 = (int8_t)(acc_val[0]);
	g_acc_data.acc_y_1 = (int8_t)(acc_val[1] >> 8);
	g_acc_data.acc_y_2 = (int8_t)(acc_val[1]);
	g_acc_data.acc_z_1 = (int8_t)(acc_val[2] >> 8);
	g_acc_data.acc_z_2 = (int8_t)(acc_val[2]);

	lmh_error_status result = send_lora_packet((uint8_t *)&g_acc_data, 8);
...
}
```
