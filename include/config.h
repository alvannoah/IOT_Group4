#pragma once

#include <stdint.h>

// Wiring (Arduino Uno)
const uint8_t FLOW_SENSOR_PIN = 2;  // Must be 2 or 3: the flow meter needs an interrupt.
const uint8_t ESP_RX_PIN = 4;       // Connects to ESP-01 TX.
const uint8_t ESP_TX_PIN = 5;       // Connects to ESP-01 RX through a 5V to 3.3V divider.
const uint8_t VALVE_PIN = 7;        // Relay or MOSFET input driving the solenoid.
const uint8_t PZEM_RX_PIN = 10;     // Connects to PZEM TX.
const uint8_t PZEM_TX_PIN = 11;     // Connects to PZEM RX.

// Level on VALVE_PIN that opens the valve. Most relay boards are active-low, so this is
// LOW for them. Use HIGH for an active-high relay or a logic-level MOSFET.
const uint8_t VALVE_OPEN_LEVEL = LOW;

const long ESP_BAUD = 9600;  // Must match the ESP-01 (AT+UART_DEF). Do not go higher.

// Pulses per litre. 450 is the YF-S201 datasheet value; calibrate against a measured
// volume of water for better accuracy.
const float FLOW_PULSES_PER_LITRE = 450.0;

// How often to talk to the backend. Keep the sensor intervals well under the backend's
// DEVICE_OFFLINE_AFTER_SECONDS (60), or the dashboard shows the sensors as offline.
const unsigned long VALVE_POLL_MS = 2000;
const unsigned long WATER_SEND_MS = 10000;
const unsigned long ELECTRICITY_SEND_MS = 10000;

// Rejoin WiFi after this many requests in a row fail, and wait this long between tries.
const uint8_t MAX_FAILED_REQUESTS = 3;
const unsigned long WIFI_RETRY_MS = 15000;
