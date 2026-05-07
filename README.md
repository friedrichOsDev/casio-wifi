# Casio fx-CP400 ESP32 Internet Bridge

This project enables internet connectivity for the Casio fx-CP400 (ClassPad II) graphing calculator by using an ESP32 as a communication bridge via the 3-pin serial port.

## Hardware Requirements
- **Casio fx-CP400**
- **ESP32 Development Board** (ESP32-WROOM-32)
- **2.5mm Stereo Jack Cable** (to connect to the Casio serial port)

## Pinout Mapping
| Casio Port | ESP32 Pin | Description |
|------------|-----------|-------------|
| Tip        | RX2 (GPIO 16) | Transmit from Casio |
| Ring       | TX2 (GPIO 17) | Receive to Casio |
| Sleeve     | GND       | Ground |

## Features
- Connects the fx-CP400 to local Wi-Fi.
- All you can implement with 115200 baud (11.52 KB/s) serial communication and your imagination! HTTP requests, custom Protocols/APIs, etc.

## Setup
- **Flash ESP32:** Upload your firmware using the Arduino IDE.

## Usage
My own Casio <-> ESP32 communication protocol can be used to send and get basic WLAN info (connect, disconnect, status, IP, etc.) and to send and receive raw binary packets (trancieve) to the ESP32. The transceived packets can be used to implement custom protocols, APIs, or even a simple terminal interface for the ESP32 on the Casio fx-CP400.

## License
MIT