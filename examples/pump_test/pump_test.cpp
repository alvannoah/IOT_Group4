// Stand-alone water pump test. No WiFi, PZEM or flow sensor needed.
// Build and upload: pio run -e pump_test -t upload && pio device monitor -e pump_test
//
// Serial commands (115200 baud, send one character):
//   o  pump on        c  pump off
//   p  pulse: pump on for 2 s, then off     ?  show status
// The pump stops by itself after MAX_RUN_MS. Never run it dry: keep the intake under water.
#include <Arduino.h>

#include "config.h"  // PUMP_PIN (8) and VALVE_OPEN_LEVEL: relay polarity is the same for every channel

// Level on PUMP_PIN that runs the pump: same relay board, same polarity as the valve.
const uint8_t PUMP_ON_LEVEL = VALVE_OPEN_LEVEL;

const unsigned long PULSE_MS = 2000;
const unsigned long MAX_RUN_MS = 10000;

bool pumpOn = false;
bool pulsing = false;
unsigned long startedAt = 0;

void setPump(bool on) {
    digitalWrite(PUMP_PIN, on ? PUMP_ON_LEVEL : !PUMP_ON_LEVEL);
    if (on && !pumpOn) startedAt = millis();
    if (on != pumpOn) Serial.println(on ? F("Pump: on") : F("Pump: off"));
    pumpOn = on;
}

void setup() {
    Serial.begin(115200);
    digitalWrite(PUMP_PIN, !PUMP_ON_LEVEL);  // Off level first, then enable the output.
    pinMode(PUMP_PIN, OUTPUT);
    Serial.println(F("Pump test ready. o=on c=off p=2s pulse ?=status"));
}

void loop() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == 'o') {
            pulsing = false;
            setPump(true);
        } else if (c == 'c') {
            pulsing = false;
            setPump(false);
        } else if (c == 'p') {
            pulsing = true;
            setPump(true);
        } else if (c == '?') {
            Serial.print(F("Pin "));
            Serial.print(PUMP_PIN);
            Serial.print(F(", pump "));
            Serial.println(pumpOn ? F("on") : F("off"));
        }
    }

    if (pulsing && millis() - startedAt >= PULSE_MS) {
        pulsing = false;
        setPump(false);
    }

    if (pumpOn && millis() - startedAt >= MAX_RUN_MS) {
        pulsing = false;
        setPump(false);
        Serial.println(F("Max run time reached"));
    }
}
