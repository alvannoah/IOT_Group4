// Stand-alone water flow sensor (YF-S201) test. No WiFi, PZEM or relay needed.
// Build and upload: pio run -e flow_test -t upload && pio device monitor -e flow_test
//
// Wiring: sensor red -> 5V, black -> GND, yellow (signal) -> D2 (FLOW_SENSOR_PIN).
// Prints once a second: pulses counted, flow rate in L/min, and total litres.
// Serial command: r resets the total.
#include <Arduino.h>

#include "config.h"  // FLOW_SENSOR_PIN and FLOW_PULSES_PER_LITRE, same as the real firmware

volatile unsigned long pulseCount = 0;
unsigned long lastReport = 0;
unsigned long pulsesAtLastReport = 0;

void countPulse() {
    pulseCount++;
}

void setup() {
    Serial.begin(115200);
    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), countPulse, FALLING);
    Serial.print(F("Flow test ready on D"));
    Serial.print(FLOW_SENSOR_PIN);
    Serial.println(F(". Blow or pour water through the sensor. r=reset total"));
    lastReport = millis();
}

void loop() {
    if (Serial.available() && Serial.read() == 'r') {
        noInterrupts();
        pulseCount = 0;
        interrupts();
        pulsesAtLastReport = 0;
        Serial.println(F("Total reset"));
    }

    const unsigned long now = millis();
    if (now - lastReport < 1000) return;

    noInterrupts();
    const unsigned long pulses = pulseCount;  // Copy with interrupts off: it is 4 bytes.
    interrupts();

    const float minutes = (now - lastReport) / 60000.0;
    const float litres = (pulses - pulsesAtLastReport) / FLOW_PULSES_PER_LITRE;
    const float flowRate = litres / minutes;
    const float total = pulses / FLOW_PULSES_PER_LITRE;

    Serial.print(F("pulses "));
    Serial.print(pulses);
    Serial.print(F("  flow "));
    Serial.print(flowRate, 2);
    Serial.print(F(" L/min  total "));
    Serial.print(total, 3);
    Serial.println(F(" L"));

    pulsesAtLastReport = pulses;
    lastReport = now;
}
