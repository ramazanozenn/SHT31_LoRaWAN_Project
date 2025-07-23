#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

// SHT31 sensor object
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// LoRaWAN Identity Information
// YOU MUST FILL THESE FIELDS WITH YOUR OWN TTN DEVICE INFORMATION!
// Example values ARE NOT YOUR CURRENT VALUES!

// DEVEUI: Reversed order (LSB first) - LSB-converted value of "End device EUI" in TTN
// Example: If TTN shows 227627B67B73E73E, write {0x3E, 0xE7, 0x73, 0x7B, 0xB6, 0x27, 0x76, 0x22} here.
static const u1_t PROGMEM DEVEUI[8] = { 0x3E, 0xE7, 0x73, 0x7B, 0xB6, 0x27, 0x76, 0x22 };

// APPEUI (JoinEUI): Reversed order (LSB first) - If you entered "0000000000000000" in TTN, it remains {0x00, 0x00, ...}.
// If TTN provided you with a different JoinEUI, use its LSB-converted value.
static const u1_t PROGMEM APPEUI[8] = { 0xFF, 0x9C, 0x59, 0x2F, 0xA9, 0xBE, 0x14, 0x13 };

// APPKEY: Straight order (MSB first) - Byte-by-byte separated value of "AppKey" in TTN
// Example: If TTN shows F0EE877C22F0A718AC7AE6911ED84732, write it as below.
static const u1_t PROGMEM APPKEY[16] = {
  0xF0, 0xEE, 0x87, 0x7C, 0x22, 0xF0, 0xA7, 0x18, // First 8 bytes
  0xAC, 0x7A, 0xE6, 0x91, 0x1E, 0xD8, 0x47, 0x32  // Last 8 bytes
};


// These functions allow the LMIC library to access identity information
void os_getArtEui(u1_t* buf)  { memcpy_P(buf, APPEUI, 8); }
void os_getDevEui(u1_t* buf)  { memcpy_P(buf, DEVEUI, 8); }
void os_getDevKey(u1_t* buf)  { memcpy_P(buf, APPKEY, 16); }

// osjob object for periodic tasks
static osjob_t sendjob;

// LMIC Pin Mapping for LoRa Shield v1.4 (Arduino UNO compatible, DIO0 to D2, DIO1 to D3)
// Please ensure these pins are correctly seated on your shield and wires are properly connected.
const lmic_pinmap lmic_pins = {
  .nss = 10,                 // NSS pin of the LoRa module (usually SPI Slave Select)
  .rxtx = LMIC_UNUSED_PIN,   // Used on some modules for antenna control, generally not used on Dragino
  .rst = 9,                  // RST pin of the LoRa module (reset)
  .dio = {2, 3, LMIC_UNUSED_PIN} // DIO0 -> D2, DIO1 -> D3, DIO2 generally not used
};

// LMIC Event Management Function
void onEvent(ev_t ev) {
  switch(ev) {
    case EV_JOINING:
      Serial.println(F("Joining TTN network...")); // Join attempt
      break;
    case EV_JOINED:
      Serial.println(F("Joined TTN network!")); // Successful join
      // Start the first data transmission immediately after joining the network
      do_send(&sendjob);
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("Data sent.")); // Data packet transmission completed
      // Set timer for the next data transmission (e.g., after 60 seconds)
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(60), do_send);
      break;
    case EV_JOIN_FAILED:
      Serial.println(F("Failed to join TTN network!")); // Join error
      // Wait a while to try again in case of error
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(30), do_send);
      break;
    case EV_TXSTART:
      Serial.println(F("Starting transmission...")); // Data transmission is about to begin
      break;
    case EV_RXSTART:
      Serial.println(F("Starting reception...")); // Waiting for response
      break;
    // case EV_JOIN_ACCEPT: // Comment out or delete this line and the code below it
    //   Serial.println(F("Join accepted."));
    //   break;
    case EV_RFU1:
      // RFU1 usually indicates LMIC's status information, not important
      break;
    default:
      // Let's not suppress other events for RAM
      break;
  }
}
// Function to read sensor data and send it as a LoRaWAN packet
void do_send(osjob_t* j) {
  // Don't send if LMIC is busy
  if (LMIC.opmode & OP_TXRXPEND) {
      Serial.println(F("Previous transmission pending, retrying..."));
      // Retry after a short delay
      os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(5), do_send);
      return;
  }

  // Read SHT31 sensor data
  Serial.println(F("--- Reading SHT31 data and packaging ---")); // Debug message
  float temp = sht31.readTemperature();
  float hum = sht31.readHumidity();

  Serial.print(F("Raw Temperature Read: ")); Serial.println(temp); // Debug message
  Serial.print(F("Raw Humidity Read: ")); Serial.println(hum);     // Debug message

  // Error checking for sensor readings
  if (isnan(temp) || isnan(hum)) {
    Serial.println(F("Could not read SHT31 data! (NaN values) Retrying..."));
    // Short delay to retry on failed read
    os_setTimedCallback(&sendjob, os_getTime() + sec2osticks(10), do_send);
    return;
  }

  // Create payload (4 bytes: temperature and humidity * 100)
  uint8_t payload[4];
  int t = (int)(temp * 100); // Convert temperature to int by multiplying by 100
  int h = (int)(hum * 100);  // Convert humidity to int by multiplying by 100

  payload[0] = t >> 8;      // High byte of temperature
  payload[1] = t & 0xFF;    // Low byte of temperature
  payload[2] = h >> 8;      // High byte of humidity
  payload[3] = h & 0xFF;    // Low byte of humidity

  // Print temperature and humidity values to serial monitor
  Serial.print(F("Temperature: "));
  Serial.print(temp);
  Serial.print(F(" °C, Humidity: "));
  Serial.print(hum);
  Serial.println(F(" %"));

  // Send LoRaWAN packet (Port 1, unconfirmed)
  // The first parameter is the port number, 1 is generally used for sensor data.
  // The last parameter is 0: unconfirmed, 1: confirmed
  LMIC_setTxData2(1, payload, sizeof(payload), 0);
  Serial.println(F("Sending data..."));
}

void setup() {
  Serial.begin(9600);
  delay(3000); // Sufficient time for the serial port to start
  Serial.println(F("Starting up...")); // POINT 1

  Wire.begin();
  Serial.println(F("I2C initialized.")); // POINT 2

  if (!sht31.begin(0x44)) {
    Serial.println(F("SHT31 not found! Please check connections and address."));
    //while (1); // If sensor is not found, it enters an infinite loop here and the program stops.
  } else {
    Serial.println(F("SHT31 successfully detected.")); // POINT 3
  }

  // Initialize LMIC and LoRa module
  Serial.println(F("Initializing OS...")); // POINT 4
  os_init();
  Serial.println(F("OS initialized."));     // POINT 5

  Serial.println(F("Resetting LMIC...")); // POINT 6
  LMIC_reset(); // Reset LMIC library
  Serial.println(F("LMIC reset."));     // POINT 7

  // LMIC_setClockError(MAX_CLOCK_ERROR * 1 / 100); // THIS LINE REMOVED!
  
  // Data rate (SF7) and power (14 dBm) settings
  LMIC_setDrTxpow(DR_SF7, 14);

  // Start the LoRaWAN network join process
  Serial.println(F("Starting LoRaWAN and attempting to join network...")); // POINT 8
}

void loop() {
  os_runloop_once(); // Run the LMIC event loop
 
}
