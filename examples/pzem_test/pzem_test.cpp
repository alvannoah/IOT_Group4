// Stand-alone PZEM-004T test. No WiFi, relays or flow sensor needed.
// Build and upload: pio run -e pzem_test -t upload && pio device monitor -e pzem_test
//
// Wiring: PZEM TX -> D10 (PZEM_RX_PIN), PZEM RX <- D11 (PZEM_TX_PIN), PZEM 5V and GND on the
// 5 V and ground junctions. The PZEM answers only when mains is also connected to its L and N
// voltage terminals: the 5 V powers the serial side, but the measuring chip runs from mains.
// Prints once a second: voltage, current, power, energy, frequency and power factor.
#include <Arduino.h>
#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>

#include "config.h"  // PZEM_RX_PIN and PZEM_TX_PIN, same as the real firmware

SoftwareSerial pzemSerial(PZEM_RX_PIN, PZEM_TX_PIN);
PZEM004Tv30 pzem(pzemSerial);

void setup() {
    Serial.begin(115200);
    pzemSerial.begin(9600);
    Serial.println(F("PZEM test ready."));
}

void loop() {
    const float voltage = pzem.voltage();

    if (isnan(voltage)) {
        Serial.println(F("No reply from the PZEM. Check: mains on its L/N terminals, 5V and GND, "
                         "and TX/RX (PZEM TX to D10, PZEM RX to D11)."));
    } else {
        Serial.print(F("Voltage: "));
        Serial.print(voltage, 1);
        Serial.print(F(" V  Current: "));
        Serial.print(pzem.current(), 3);
        Serial.print(F(" A  Power: "));
        Serial.print(pzem.power(), 1);
        Serial.print(F(" W  Energy: "));
        Serial.print(pzem.energy(), 3);
        Serial.print(F(" kWh  Freq: "));
        Serial.print(pzem.frequency(), 1);
        Serial.print(F(" Hz  PF: "));
        Serial.println(pzem.pf(), 2);
    }
    delay(1000);
}
