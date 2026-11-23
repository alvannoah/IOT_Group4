// Stand-alone test of the solenoid valve (D7) and water pump (D8) together.
// No WiFi, PZEM or flow sensor needed.
// Build and upload: pio run -e pump_valve_test -t upload && pio device monitor -e pump_valve_test
//
// Serial commands (115200 baud, send one character):
//   v  toggle the valve     p  toggle the pump     x  everything off
//   a  run the demo sequence: valve opens, pump runs, pump stops, valve closes
//   ?  show status
// Each output has a time limit, so a forgotten test can't run on. Keep the pump's intake
// under water: it must not run dry.
#include <Arduino.h>

#include "config.h"  // VALVE_PIN (7) and VALVE_OPEN_LEVEL, used as the relay polarity for both

// Level on PUMP_PIN that runs the pump: same relay board, same polarity as the valve.
const uint8_t PUMP_ON_LEVEL = VALVE_OPEN_LEVEL;

const unsigned long MAX_VALVE_MS = 30000;
const unsigned long MAX_PUMP_MS = 10000;

// Demo sequence: each step lasts STEP_MS.
const unsigned long STEP_MS = 3000;

bool valveOpen = false;
bool pumpOn = false;
unsigned long valveOpenedAt = 0;
unsigned long pumpStartedAt = 0;

uint8_t step = 0;  // 0 = no sequence running, 1..4 = sequence steps.
unsigned long stepStartedAt = 0;

void setValve(bool open) {
    digitalWrite(VALVE_PIN, open ? VALVE_OPEN_LEVEL : !VALVE_OPEN_LEVEL);
    if (open && !valveOpen) valveOpenedAt = millis();
    if (open != valveOpen) Serial.println(open ? F("Valve: open") : F("Valve: closed"));
    valveOpen = open;
}

void setPump(bool on) {
    digitalWrite(PUMP_PIN, on ? PUMP_ON_LEVEL : !PUMP_ON_LEVEL);
    if (on && !pumpOn) pumpStartedAt = millis();
    if (on != pumpOn) Serial.println(on ? F("Pump: on") : F("Pump: off"));
    pumpOn = on;
}

void allOff() {
    step = 0;
    setPump(false);
    setValve(false);
}

// The demo: 1 valve opens, 2 pump runs too, 3 pump stops, 4 valve closes, then it ends.
void runStep(uint8_t s) {
    step = s;
    stepStartedAt = millis();
    switch (s) {
        case 1: setValve(true); break;
        case 2: setPump(true); break;
        case 3: setPump(false); break;
        case 4: setValve(false); break;
        default: step = 0; Serial.println(F("Sequence done")); break;
    }
}

void setup() {
    Serial.begin(115200);
    // Off levels first, then enable the outputs, so nothing switches on at boot.
    digitalWrite(VALVE_PIN, !VALVE_OPEN_LEVEL);
    digitalWrite(PUMP_PIN, !PUMP_ON_LEVEL);
    pinMode(VALVE_PIN, OUTPUT);
    pinMode(PUMP_PIN, OUTPUT);
    Serial.println(F("Pump + valve test ready. v=valve p=pump a=sequence x=all off ?=status"));
}

void loop() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == 'v') {
            step = 0;
            setValve(!valveOpen);
        } else if (c == 'p') {
            step = 0;
            setPump(!pumpOn);
        } else if (c == 'x') {
            allOff();
        } else if (c == 'a') {
            allOff();
            Serial.println(F("Sequence start"));
            runStep(1);
        } else if (c == '?') {
            Serial.print(F("Valve (D"));
            Serial.print(VALVE_PIN);
            Serial.print(F(") "));
            Serial.print(valveOpen ? F("open") : F("closed"));
            Serial.print(F(", pump (D"));
            Serial.print(PUMP_PIN);
            Serial.print(F(") "));
            Serial.println(pumpOn ? F("on") : F("off"));
        }
    }

    if (step != 0 && millis() - stepStartedAt >= STEP_MS) runStep(step + 1);

    if (valveOpen && millis() - valveOpenedAt >= MAX_VALVE_MS) {
        setValve(false);
        Serial.println(F("Valve max open time reached"));
    }
    if (pumpOn && millis() - pumpStartedAt >= MAX_PUMP_MS) {
        setPump(false);
        Serial.println(F("Pump max run time reached"));
    }
}
