#include "esp_http.h"

#include <Arduino.h>
#include <SoftwareSerial.h>

#include "config.h"
#include "http_reply.h"
#include "secrets.h"

static SoftwareSerial espSerial(ESP_RX_PIN, ESP_TX_PIN);

// Long enough for the electricity URL plus headers. Static so that it shows up in the
// build's RAM figure instead of quietly eating into the stack.
static char request[220];
static HttpReply reply;

// Waits for `token`. Gives up early if the ESP reports ERROR or FAIL.
static bool waitFor(const char *token, unsigned long timeoutMs) {
    const char *const failures[] = {"ERROR", "FAIL"};
    uint8_t tokenMatch = 0;
    uint8_t failureMatch[] = {0, 0};

    const unsigned long started = millis();
    while (millis() - started < timeoutMs) {
        if (!espSerial.available()) continue;
        const char c = espSerial.read();

        tokenMatch = (c == token[tokenMatch]) ? tokenMatch + 1 : (c == token[0] ? 1 : 0);
        if (token[tokenMatch] == '\0') return true;

        for (uint8_t i = 0; i < 2; i++) {
            const char *failure = failures[i];
            uint8_t &match = failureMatch[i];
            match = (c == failure[match]) ? match + 1 : (c == failure[0] ? 1 : 0);
            if (failure[match] == '\0') return false;
        }
    }
    return false;
}

static void drainInput() {
    while (espSerial.available()) espSerial.read();
}

static bool sendCommand(const __FlashStringHelper *command, const char *token,
                        unsigned long timeoutMs) {
    drainInput();
    espSerial.println(command);
    return waitFor(token, timeoutMs);
}

static void closeConnection() {
    sendCommand(F("AT+CIPCLOSE"), "OK", 1000);
}

void espBegin() {
    espSerial.begin(ESP_BAUD);
}

bool espConnectWifi() {
    // The PZEM library switches the shared software serial listener to its own port.
    espSerial.listen();

    // A failed wait here is fine: not every firmware prints "ready", and the next command
    // shows whether the module is alive.
    sendCommand(F("AT+RST"), "ready", 6000);
    delay(500);

    if (!sendCommand(F("ATE0"), "OK", 2000) ||
        !sendCommand(F("AT+CWMODE=1"), "OK", 2000) ||
        !sendCommand(F("AT+CIPMUX=0"), "OK", 2000)) {
        Serial.println(F("ESP: no reply to AT commands; check wiring and baud rate"));
        return false;
    }

    drainInput();
    espSerial.print(F("AT+CWJAP=\""));
    espSerial.print(F(WIFI_SSID));
    espSerial.print(F("\",\""));
    espSerial.print(F(WIFI_PASSWORD));
    espSerial.println('"');
    if (!waitFor("OK", 20000)) {
        Serial.println(F("ESP: could not join WiFi; check SSID and password"));
        return false;
    }

    Serial.println(F("ESP: WiFi connected"));
    return true;
}

int espGet(const char *path, char *body, size_t bodySize) {
    const int length = snprintf_P(request, sizeof(request),
                                  PSTR("GET %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\n\r\n"),
                                  path, BACKEND_HOST, BACKEND_PORT);
    if (length < 0 || length >= (int)sizeof(request)) {
        Serial.println(F("ESP: request too long"));
        return HTTP_FAILED;
    }

    espSerial.listen();
    drainInput();

    espSerial.print(F("AT+CIPSTART=\"TCP\",\""));
    espSerial.print(F(BACKEND_HOST));
    espSerial.print(F("\","));
    espSerial.println(BACKEND_PORT);
    if (!waitFor("OK", 8000)) {
        closeConnection();  // Clears an "ALREADY CONNECTED" left by an earlier failure.
        return HTTP_FAILED;
    }

    espSerial.print(F("AT+CIPSEND="));
    espSerial.println(length);
    if (!waitFor(">", 2000)) {
        closeConnection();
        return HTTP_FAILED;
    }
    espSerial.print(request);

    // Don't wait for "SEND OK": the reply can arrive before it, and HttpReply skips
    // anything outside a data frame. Stay in this loop without printing, as the receive
    // buffer is only 64 bytes.
    reply.reset();
    const unsigned long started = millis();
    while (!reply.closed() && millis() - started < 5000) {
        if (espSerial.available()) reply.feed(espSerial.read());
    }

    if (!reply.closed()) closeConnection();
    if (!reply.complete() || reply.status() == 0) return HTTP_FAILED;

    strlcpy(body, reply.body(), bodySize);
    return reply.status();
}
