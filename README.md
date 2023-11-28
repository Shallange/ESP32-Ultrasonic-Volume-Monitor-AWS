# IoT Volume Measurement System

## Overview
This project develops an IoT-based volume measurement system using ESP32, HC-SR04 ultrasonic sensor, and AWS IoT. It calculates and transmits the volume data of cuboid or cylinder containers based on the distance measured by the HC-SR04 sensor.

## Hardware Requirements
- ESP32 Development Board
- HC-SR04 Ultrasonic Sensor
- Jumper Wires
- Power Supply for ESP32

## Software Requirements
- Arduino IDE
- AWS Account and IoT Core setup
- Libraries: WiFiClientSecure, MQTTClient, ArduinoJson

## Setup and Configuration
### 1. WiFi and AWS Configuration
Update `credentials.h` with your WiFi and AWS credentials.

### 2. Hardware Connections
Connect the HC-SR04 sensor's trigger pin to GPIO 5 and echo pin to GPIO 4 of the ESP32.

### 3. AWS IoT Setup
Create a thing in AWS IoT, download certificates, and update them in the code.

### 4. Arduino IDE Setup
Open the code in Arduino IDE, select ESP32 as the board, and choose the correct COM port.

### 5. Upload the Code
Compile and upload the code to the ESP32 board.

## Operation
The system operates in two modes:
- **Cuboid Mode:** For cuboid container volume calculation.
- **Cylinder Mode:** For cylindrical container volume calculation.

The device measures the distance to the liquid surface and calculates the volume.
