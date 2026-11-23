// Stand-alone solenoid valve test. No WiFi, PZEM or flow sensor needed.
// Build and upload: pio run -e valve_test -t upload && pio device monitor -e valve_test
//
// Serial commands (115200 baud, send one character):
//   o  open the valve        c  close the valve
//   a  toggle auto-cycle (3 s open, 3 s closed)    ?  show status
// The valve closes by itself after MAX_OPEN_MS, so a forgotten test can't flood anything.
#include <Arduino.h>

#include "config.h"  // VALVE_PIN and VALVE_OPEN_LEVEL, same as the real firmware

const unsigned long CYCLE_MS = 3000;
const unsigned long MAX_OPEN_MS = 30000;

bool valveOpen = false;
bool autoCycle = false;
unsigned long openedAt = 0;
unsigned long lastToggle = 0;

void setValve(bool open) {
    digitalWrite(VALVE_PIN, open ? VALVE_OPEN_LEVEL : !VALVE_OPEN_LEVEL);
    if (open && !valveOpen) openedAt = millis();
    if (open != valveOpen) Serial.println(open ? F("Valve: open") : F("Valve: closed"));
    valveOpen = open;
}

void setup() {
    Serial.begin(115200);
    digitalWrite(VALVE_PIN, !VALVE_OPEN_LEVEL);  // Closed level first, then enable the output.
    pinMode(VALVE_PIN, OUTPUT);
    Serial.println(F("Valve test ready. o=open c=close a=auto ?=status"));
}

void loop() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == 'o') {
            autoCycle = false;
            setValve(true);
        } else if (c == 'c') {
            autoCycle = false;
            setValve(false);
        } else if (c == 'a') {
            autoCycle = !autoCycle;
            lastToggle = millis();
            Serial.println(autoCycle ? F("Auto-cycle on") : F("Auto-cycle off"));
        } else if (c == '?') {
            Serial.print(F("Pin "));
            Serial.print(VALVE_PIN);
            Serial.print(F(", valve "));
            Serial.print(valveOpen ? F("open") : F("closed"));
            Serial.print(F(", auto "));
            Serial.println(autoCycle ? F("on") : F("off"));
        }
    }

    if (autoCycle && millis() - lastToggle >= CYCLE_MS) {
        setValve(!valveOpen);
        lastToggle = millis();
    }

    if (valveOpen && millis() - openedAt >= MAX_OPEN_MS) {
        autoCycle = false;
        setValve(false);
        Serial.println(F("Max open time reached"));
    }
}
