# SHT31 Temperature and Humidity Sensor with LoRaWAN

This project reads temperature and humidity data from the SHT31 sensor via I2C and transmits the data over a LoRaWAN network (e.g., The Things Network - TTN). It demonstrates sensor interfacing, data encoding, and wireless communication using LoRa technology.

---

## Hardware Requirements

- Microcontroller board with LoRaWAN support (e.g., Arduino + LoRa shield, ESP32 + LoRa module)
- SHT31 temperature and humidity sensor
- Connecting wires (I2C: SDA, SCL, VCC, GND)
- Power supply for the microcontroller

---

## Software Requirements

- Arduino IDE (or PlatformIO)
- Required Arduino libraries:
  - `Wire.h` (for I2C communication)
  - LoRaWAN related libraries (e.g., `arduino-lmic`)
  - SHT31 sensor library (e.g., from Adafruit or SparkFun)

---

## Setup and Installation

1. Connect the SHT31 sensor to your microcontroller via I2C pins (usually SDA, SCL, VCC, GND).
2. Attach the LoRa module/shield to your microcontroller.
3. Configure your device credentials (DevEUI, AppEUI, AppKey) in the source code or a separate configuration file.
4. Upload the Arduino sketch to your device using the Arduino IDE.
5. Join your LoRaWAN network (e.g., TTN).

---

## Usage

- The device reads temperature and humidity values from the SHT31 sensor periodically.
- The sensor data is encoded and sent as uplink messages via LoRaWAN.
- Use the included TTN decoder function (see below) to decode sensor data on The Things Network console.

---

## TTN Decoder Function

```javascript
function decodeUplink(input) {
  const data = input.bytes;
  
  const temperature = ((data[0] << 8) | data[1]) * 0.01;
  const humidity = ((data[2] << 8) | data[3]) * 0.01;
  
  return {
    data: {
      temperature: temperature,
      humidity: humidity.toFixed(2)
    }
  };
}
