// Reads the PZEM-004T and YF-S201, sends the readings to the FastAPI backend through the
// ESP-01, and switches the water (solenoid valve and pump together) and the LED strip as the
// backend says.
#include <Arduino.h>
#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>

#include "config.h"
#include "esp_http.h"

SoftwareSerial pzemSerial(PZEM_RX_PIN, PZEM_TX_PIN);
PZEM004Tv30 pzem(pzemSerial);

struct Reading {
    float voltage;
    float current;
    float power;
    float energy;
    float frequency;
    float pf;
};

volatile unsigned long pulseCount = 0;

unsigned long pulsesAtLastSend = 0;
unsigned long lastWaterSend = 0;
unsigned long lastElectricitySend = 0;
unsigned long lastValvePoll = 0;
unsigned long lastWifiAttempt = 0;
unsigned long lastBackendReply = 0;
unsigned long pumpStartedAt = 0;

bool wifiConnected = false;
bool valveOpen = false;
bool pumpOn = false;
bool waterLockedOut = false;  // Pump hit its run-time limit; water stays off until the backend says off.
bool ledOn = false;
uint8_t failedRequests = 0;

// Long enough for the largest values each sensor can report.
char path[140];

void countPulse() {
    pulseCount++;
}

// ---------------------------------------------------------------- Relay outputs

void setRelay(uint8_t pin, bool on) {
    digitalWrite(pin, on ? VALVE_OPEN_LEVEL : !VALVE_OPEN_LEVEL);
}

void setValve(bool open) {
    setRelay(VALVE_PIN, open);
    if (open != valveOpen) {
        Serial.println(open ? F("Valve: open") : F("Valve: closed"));
    }
    valveOpen = open;
}

void setPump(bool on) {
    setRelay(PUMP_PIN, on);
    if (on && !pumpOn) pumpStartedAt = millis();
    if (on != pumpOn) {
        Serial.println(on ? F("Pump: on") : F("Pump: off"));
    }
    pumpOn = on;
}

void setLedStrip(bool on) {
    setRelay(LED_STRIP_PIN, on);
    if (on != ledOn) {
        Serial.println(on ? F("LED strip: on") : F("LED strip: off"));
    }
    ledOn = on;
}

// The valve and pump are one switch: the backend's single /api/valve setting drives both.
void setWater(bool on) {
    setValve(on);
    setPump(on);
}

// Checked every pass of loop(), including while WiFi is down.
void enforcePumpLimits() {
    if (!pumpOn) return;

    if (millis() - pumpStartedAt >= PUMP_MAX_RUN_MS) {
        Serial.println(F("Pump: run-time limit reached, closing the valve too"));
        waterLockedOut = true;
        setWater(false);
    } else if (millis() - lastBackendReply >= PUMP_FAILSAFE_MS) {
        Serial.println(F("Pump: no contact with the backend, stopping"));
        setPump(false);
    }
}

// ---------------------------------------------------------------- Backend

// Tracks consecutive failures so a dead connection triggers a WiFi rejoin. Any HTTP reply
// counts as success: a 422 means the network is fine and the reading was refused.
int backendGet(const char *requestPath, char *body, size_t bodySize) {
    const int status = espGet(requestPath, body, bodySize);

    if (status == HTTP_FAILED) {
        Serial.print(F("Request failed: "));
        Serial.println(requestPath);
        if (++failedRequests >= MAX_FAILED_REQUESTS) {
            Serial.println(F("Too many failures; rejoining WiFi"));
            wifiConnected = false;
            failedRequests = 0;
        }
    } else {
        failedRequests = 0;
        lastBackendReply = millis();
        if (status != 200) {
            Serial.print(F("Backend replied "));
            Serial.print(status);
            Serial.print(F(" to "));
            Serial.println(requestPath);
        }
    }
    return status;
}

// Asks the backend for a 0/1 setting. Returns 1 or 0 as it says, or -1 if there was no usable
// reply (no connection, an error status such as 404, or an unexpected body).
int pollFlag(const char *requestPath) {
    char body[8];
    if (backendGet(requestPath, body, sizeof(body)) != 200) return -1;
    if (strcmp(body, "1") == 0) return 1;
    if (strcmp(body, "0") == 0) return 0;
    return -1;
}

void pollOutputs() {
    // With no usable reply an output stays as it is, so a WiFi dropout doesn't cut off the
    // water or leave a relay unexpectedly switched. The pump is the exception: it has its own
    // failsafe in enforcePumpLimits().
    const int water = pollFlag("/api/valve");
    if (water == 0) {
        waterLockedOut = false;
        setWater(false);
    } else if (water == 1 && !waterLockedOut) {
        setWater(true);
    }

    const int led = pollFlag("/api/led");
    if (led >= 0) setLedStrip(led == 1);
}

// ---------------------------------------------------------------- YF-S201

void sendWater() {
    noInterrupts();
    const unsigned long pulses = pulseCount;
    interrupts();

    const unsigned long now = millis();
    const float minutes = (now - lastWaterSend) / 60000.0;
    const float litres = (pulses - pulsesAtLastSend) / FLOW_PULSES_PER_LITRE;
    const float flowRate = minutes > 0 ? litres / minutes : 0;
    const float totalVolume = pulses / FLOW_PULSES_PER_LITRE;

    char flowText[16], totalText[16];
    dtostrf(flowRate, 1, 2, flowText);
    dtostrf(totalVolume, 1, 3, totalText);
    snprintf_P(path, sizeof(path), PSTR("/api/device/water?flow_rate=%s&total_volume=%s"),
               flowText, totalText);

    Serial.print(F("Water: "));
    Serial.print(flowText);
    Serial.print(F(" L/min, total "));
    Serial.print(totalText);
    Serial.println(F(" L"));

    char body[8];
    if (backendGet(path, body, sizeof(body)) == 200) {
        // Moved on only after a success, so water used during an outage is still counted
        // and shows up as an average rate over the outage.
        pulsesAtLastSend = pulses;
        lastWaterSend = now;
    }
}

// ---------------------------------------------------------------- PZEM-004T

bool readValid(Reading &r) {
    r.voltage = pzem.voltage();
    r.current = pzem.current();
    r.power = pzem.power();
    r.energy = pzem.energy();
    r.frequency = pzem.frequency();
    r.pf = pzem.pf();

    return !isnan(r.voltage) && r.voltage >= 80 && r.voltage <= 260 &&
           !isnan(r.current) && r.current >= 0 && r.current <= 100 &&
           !isnan(r.power) && r.power >= 0 && r.power <= 26000 &&
           !isnan(r.energy) && r.energy >= 0 &&
           !isnan(r.frequency) && r.frequency >= 45 && r.frequency <= 65 &&
           !isnan(r.pf) && r.pf >= 0 && r.pf <= 1;
}

bool getAveragedReading(Reading &average) {
    const byte samplesNeeded = 3;
    byte count = 0;
    float v = 0, a = 0, w = 0, hz = 0, pf = 0;
    float energy = 0;

    // Short enough that a disconnected meter doesn't hold up the valve for long.
    const unsigned long started = millis();
    while (count < samplesNeeded && millis() - started < 3000) {
        Reading r;

        if (readValid(r)) {
            v += r.voltage;
            a += r.current;
            w += r.power;
            hz += r.frequency;
            pf += r.pf;
            energy = r.energy;  // A running total, so the latest value is the right one.
            count++;
        }

        delay(300);
    }

    if (count == 0) return false;

    average.voltage = v / count;
    average.current = a / count;
    average.power = w / count;
    average.energy = energy;
    average.frequency = hz / count;
    average.pf = pf / count;
    return true;
}

void sendElectricity() {
    Reading r;
    if (!getAveragedReading(r)) {
        Serial.println(F("PZEM: no valid reply; check wiring and mains power"));
        return;
    }

    char voltage[16], current[16], power[16], energy[16], frequency[16], pf[16];
    dtostrf(r.voltage, 1, 1, voltage);
    dtostrf(r.current, 1, 3, current);
    dtostrf(r.power, 1, 1, power);
    dtostrf(r.energy, 1, 3, energy);
    dtostrf(r.frequency, 1, 1, frequency);
    dtostrf(r.pf, 1, 2, pf);

    snprintf_P(path, sizeof(path),
               PSTR("/api/device/electricity?voltage=%s&current=%s&power=%s&energy=%s"
                    "&frequency=%s&power_factor=%s"),
               voltage, current, power, energy, frequency, pf);

    Serial.print(F("Electricity: "));
    Serial.print(voltage);
    Serial.print(F(" V, "));
    Serial.print(power);
    Serial.print(F(" W, "));
    Serial.print(energy);
    Serial.println(F(" kWh"));

    char body[8];
    backendGet(path, body, sizeof(body));
}

// ----------------------------------------------------------------

void setup() {
    // Set the level before enabling each output, so nothing switches on at boot. Everything
    // stays off until the backend says otherwise, which matches the backend's default.
    digitalWrite(VALVE_PIN, !VALVE_OPEN_LEVEL);
    digitalWrite(PUMP_PIN, !VALVE_OPEN_LEVEL);
    digitalWrite(LED_STRIP_PIN, !VALVE_OPEN_LEVEL);
    pinMode(VALVE_PIN, OUTPUT);
    pinMode(PUMP_PIN, OUTPUT);
    pinMode(LED_STRIP_PIN, OUTPUT);

    Serial.begin(115200);
    Serial.println(F("IOT node starting"));

    pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), countPulse, FALLING);

    pzemSerial.begin(9600);
    espBegin();

    lastWaterSend = millis();
    lastBackendReply = millis();
    wifiConnected = espConnectWifi();
    lastWifiAttempt = millis();
}

void loop() {
    enforcePumpLimits();

    if (!wifiConnected) {
        if (millis() - lastWifiAttempt >= WIFI_RETRY_MS) {
            wifiConnected = espConnectWifi();
            lastWifiAttempt = millis();
        }
        return;
    }

    if (millis() - lastValvePoll >= VALVE_POLL_MS) {
        pollOutputs();
        lastValvePoll = millis();
    }
    if (wifiConnected && millis() - lastWaterSend >= WATER_SEND_MS) {
        sendWater();
    }
    if (wifiConnected && millis() - lastElectricitySend >= ELECTRICITY_SEND_MS) {
        sendElectricity();
        lastElectricitySend = millis();
    }
}
