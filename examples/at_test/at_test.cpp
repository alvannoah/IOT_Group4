// ESP-01 AT command test. At boot it tries several baud rates to find the one the ESP-01
// answers at, then everything you type in the serial monitor goes to the ESP-01 and
// everything it answers is shown. No relays, PZEM or flow sensor needed.
// Build and upload: pio run -e at_test -t upload && pio device monitor -e at_test
//
// Wiring: ESP-01 TX -> D4 (ESP_RX_PIN), ESP-01 RX <- D5 (ESP_TX_PIN) through the 1k/2k divider,
// ESP-01 VCC, CH_PD/EN and RST on 3.3 V, GND on the common ground (joined to the Arduino GND).
// In the serial monitor set 115200 baud and line ending "Both NL & CR".
// Try, one at a time: AT   AT+GMR   AT+CWMODE=1   AT+CWLAP   AT+CWJAP="ssid","password"
//
// To make the main firmware's speed (ESP_BAUD, 9600) stick, once connected send:
//   AT+UART_DEF=9600,8,1,0,0      (then restart this sketch)
// SoftwareSerial is unreliable at 115200 on an Uno, so 9600 is what the real firmware uses.
#include <Arduino.h>
#include <SoftwareSerial.h>

#include "config.h"  // ESP_RX_PIN, ESP_TX_PIN, ESP_BAUD, same as the real firmware

SoftwareSerial esp(ESP_RX_PIN, ESP_TX_PIN);

const long BAUDS[] = {ESP_BAUD, 115200, 57600, 74880, 38400, 19200};
const uint8_t BAUD_COUNT = sizeof(BAUDS) / sizeof(BAUDS[0]);

// Sends AT at the given speed and returns true if the reply contains "OK".
bool answersAt(long baud) {
    esp.begin(baud);
    esp.listen();
    delay(100);
    while (esp.available()) esp.read();  // Drop boot noise.
    esp.print(F("AT\r\n"));

    String reply;
    const unsigned long start = millis();
    while (millis() - start < 700) {
        while (esp.available()) reply += (char)esp.read();
    }
    return reply.indexOf(F("OK")) >= 0;
}

void setup() {
    Serial.begin(115200);
    Serial.println(F("ESP-01 AT test. Looking for the ESP-01's baud rate..."));
    delay(1500);  // Let the ESP-01 finish booting.

    long found = 0;
    for (uint8_t i = 0; i < BAUD_COUNT && !found; i++) {
        Serial.print(F("Trying "));
        Serial.print(BAUDS[i]);
        Serial.print(F("... "));
        if (answersAt(BAUDS[i])) {
            found = BAUDS[i];
            Serial.println(F("OK"));
        } else {
            Serial.println(F("no answer"));
        }
    }

    if (found) {
        Serial.print(F("ESP-01 answers at "));
        Serial.print(found);
        Serial.println(F(" baud. Type AT commands now."));
        esp.begin(found);
    } else {
        Serial.println(F("No answer at any speed. Check power, CH_PD, TX/RX, and the shared ground."));
        esp.begin(ESP_BAUD);
    }
}

// Typed text is collected into a line and sent to the ESP-01 only when you press Enter.
// SoftwareSerial blocks while it sends a byte, and the monitor sends a pasted line at 115200
// all at once, so forwarding byte by byte would drop characters from the middle of the command.
char line[128];
uint8_t lineLength = 0;

void loop() {
    while (esp.available()) Serial.write(esp.read());

    while (Serial.available()) {
        const char c = Serial.read();
        if (c == '\r' || c == '\n') {
            if (lineLength > 0) {
                line[lineLength] = '\0';
                esp.print(line);
                esp.print(F("\r\n"));
                lineLength = 0;
            }
        } else if (lineLength < sizeof(line) - 1) {
            line[lineLength++] = c;
        }
    }
}
