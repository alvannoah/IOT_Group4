// Stand-alone test of the solenoid valve (D7), water pump (D8), LED strip (D9) and flow sensor
// (D2) together.
// No WiFi or PZEM needed.
// Build and upload: pio run -e water_test -t upload && pio device monitor -e water_test
//
// Serial commands (115200 baud, send one character):
//   v  toggle the valve     p  toggle the pump     l  toggle the LED strip
//   w  toggle valve and pump together, as the real firmware does
//   x  everything off
//   a  demo sequence: valve opens, pump runs, pump stops, valve closes
//   r  reset the flow total    ?  show status
// Once a second it prints the valve, pump and LED state, flow pulses, flow rate and total volume.
// Each output has a time limit. Keep the pump's intake under water: it must not run dry.
#include <Arduino.h>

#include "config.h"  // Pins, VALVE_OPEN_LEVEL and FLOW_PULSES_PER_LITRE, same as the real firmware

const uint8_t PUMP_ON_LEVEL = VALVE_OPEN_LEVEL;   // Same relay board, same polarity.

const unsigned long MAX_VALVE_MS = 30000;
const unsigned long MAX_PUMP_MS = 10000;
const unsigned long STEP_MS = 3000;  // Length of each demo sequence step.
const unsigned long REPORT_MS = 1000;

volatile unsigned long pulseCount = 0;
unsigned long pulsesAtLastReport = 0;
unsigned long lastReport = 0;

bool valveOpen = false;
bool pumpOn = false;
bool ledOn = false;
unsigned long valveOpenedAt = 0;
unsigned long pumpStartedAt = 0;

uint8_t step = 0;  // 0 = no sequence running, 1..4 = sequence steps.
unsigned long stepStartedAt = 0;

void countPulse() {
    pulseCount++;
}

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

void setLed(bool on) {
    digitalWrite(LED_STRIP_PIN, on ? VALVE_OPEN_LEVEL : !VALVE_OPEN_LEVEL);
    if (on != ledOn) Serial.println(on ? F("LED strip: on") : F("LED strip: off"));
    ledOn = on;
}

void allOff() {
    step = 0;
    setLed(false);
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

unsigned long readPulses() {
    noInterrupts();
    const unsigned long p = pulseCount;  // 4 bytes: copy with interrupts off.
    interrupts();
    return p;
}

void report() {
    const unsigned long now = millis();
    const unsigned long pulses = readPulses();
    const float minutes = (now - lastReport) / 60000.0;
    const float flowRate = (pulses - pulsesAtLastReport) / FLOW_PULSES_PER_LITRE / minutes;

    Serial.print(F("valve "));
    Serial.print(valveOpen ? F("open  ") : F("closed"));
    Serial.print(F("  pump "));
    Serial.print(pumpOn ? F("on ") : F("off"));
    Serial.print(F("  led "));
    Serial.print(ledOn ? F("on ") : F("off"));
    Serial.print(F("  pulses "));
    Serial.print(pulses);
    Serial.print(F("  flow "));
    Serial.print(flowRate, 2);
    Serial.print(F(" L/min  total "));
    Serial.print(pulses / FLOW_PULSES_PER_LITRE, 3);
    Serial.println(F(" L"));

    pulsesAtLastReport = pulses;
    lastReport = now;
}

void setup() {
    Serial.begin(115200);
    // Off levels first, then enable the outputs, so nothing switches on at boot.
    digitalWrite(VALVE_PIN, !VALVE_OPEN_LEVEL);
    digitalWrite(PUMP_PIN, !PUMP_ON_LEVEL);
    digitalWrite(LED_STRIP_PIN, !VALVE_OPEN_LEVEL);
    pinMode(VALVE_PIN, OUTPUT);
    pinMode(PUMP_PIN, OUTPUT);
    pinMode(LED_STRIP_PIN, OUTPUT);
    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), countPulse, FALLING);
    Serial.println(F("Water test ready. v=valve p=pump w=both l=led a=sequence x=all off r=reset total ?=status"));
    lastReport = millis();
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
        } else if (c == 'w') {
            step = 0;
            const bool on = !(valveOpen && pumpOn);
            setValve(on);
            setPump(on);
        } else if (c == 'l') {
            setLed(!ledOn);
        } else if (c == 'x') {
            allOff();
        } else if (c == 'a') {
            allOff();
            Serial.println(F("Sequence start"));
            runStep(1);
        } else if (c == 'r') {
            noInterrupts();
            pulseCount = 0;
            interrupts();
            pulsesAtLastReport = 0;
            Serial.println(F("Total reset"));
        } else if (c == '?') {
            report();
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

    if (millis() - lastReport >= REPORT_MS) report();
}
