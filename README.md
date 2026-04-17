# GY-33 Arduino Library
An easy-to-use Arduino library for the GY-33 (TCS34725) Colour Sensor module.   This library is a C++ port of the [MicroPython GY-33 library](https://github.com/QuirkyCort/micropython-gy33) by QuirkyCort.  
This is for connecting to the GY33's microcontroller (typically STM32), not for direct connection to the TCS34725 color sensor.  
This library supports both **I2C** and **UART** (Serial) communication modes.

---

## Credits & Technical Documentation
This library is a C++ port of the original logic developed by **QuirkyCort**. 

Since the GY-33 documentation is often difficult to find in English, please refer to the original repository here: [micropython-gy33](https://github.com/QuirkyCort/micropython-gy33/tree/main)

Also:  
[UART Mode overview](https://github.com/QuirkyCort/micropython-gy33/blob/main/gy33-uart/README.md)  
[UART Mode Protocol Details](https://github.com/QuirkyCort/micropython-gy33/blob/main/gy33-uart/gy33_uart_protocol.md)  

[I2C Mode overview](https://github.com/QuirkyCort/micropython-gy33/blob/main/gy33-i2c/README.md)  
[I2C Mode Protocol Details](https://github.com/QuirkyCort/micropython-gy33/blob/main/gy33-i2c/gy33_i2c_protocol.md)

---

## Hardware Setup
The GY-33 can be powered by **3.3V or 5V** via the VCC pin. However, the data pins are **3.3V logic level**.  
*   3.3V boards (eg. ESP32 / ESP8266) can Connect directly.
*   for 5V boards (eg. Arduino Uno / Mega (5V)) please Use a logic level shifter on the I2C (SDA/SCL) or UART (TX/RX) lines to protect the sensor.  
### Mode Selection (S0 Pin)
The **S0** jumper/pin determines the primary communication mode:  
Both modes use pins 2 and 3 (CT and DR)  
*   **I2C Mode**: S0 = **GND** (Jumper closed). The module acts as an I2C device. (connect DR to SDA and CT to SCL)
*   **UART Mode**: S0 = **High/Floating** (Jumper open). The module sends data via Serial. (connect DR to TX and CT to RX)

*Note: S1 is used for direct sensor access (TCS34725) via I2C and is left open when using this library. This mode uses pins 7 and 8 (SDA and SCL). Use it with a TCS34725 library such as [Adafruit TCS34725 Driver](https://github.com/adafruit/Adafruit_TCS34725)*

---

## Quick Start  
### 1. I2C Mode
Ensure S0 is connected to **GND**.
```cpp
#include <GY33.h>

GY33_I2C sensor;

void setup() {
  Serial.begin(115200);
  sensor.begin(); // Uses default SDA/SCL pins
}

void loop() {
  if (sensor.update()) {
    Serial.print("Colour: ");
    Serial.println(sensor.colour());
  }
  delay(500);
}
```
### 2. UART Mode
Ensure S0 is **Open/High**.
```cpp
#include <GY33.h>

GY33_UART sensor(Serial1);

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, 16, 17); // Adjust pins for your board
  sensor.begin();
}

void loop() {
  if (sensor.update()) {
    Serial.print("Colour: ");
    Serial.println(sensor.colour());
  }
  delay(500);
}
```
