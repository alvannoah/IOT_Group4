// Pulls the HTTP status and body out of the ESP-01's AT output.
//
// The ESP mixes its own messages in with the data and wraps each TCP segment as
// "+IPD,<length>:<bytes>". A single reply can span several frames, so the framing has to
// be removed before looking for the body. Nothing here depends on Arduino, which is what
// lets test/test_http_reply run on a computer.
#pragma once

#include <stdint.h>

class HttpReply {
public:
    HttpReply() { reset(); }

    void reset() {
        frame_ = SCANNING;
        ipdMatch_ = closedMatch_ = 0;
        frameLength_ = 0;
        http_ = STATUS_VERSION;
        status_ = 0;
        statusDigits_ = 0;
        headerEndMatch_ = 0;
        bodyLength_ = 0;
        body_[0] = '\0';
        closed_ = false;
    }

    // Feed every byte the ESP sends after the request.
    void feed(char c) {
        switch (frame_) {
            case SCANNING:
                if (matches("+IPD,", ipdMatch_, c)) {
                    frameLength_ = 0;
                    frame_ = FRAME_LENGTH;
                } else if (matches("CLOSED", closedMatch_, c)) {
                    closed_ = true;
                }
                break;
            case FRAME_LENGTH:
                if (c >= '0' && c <= '9') {
                    frameLength_ = frameLength_ * 10 + (c - '0');
                } else {
                    frame_ = (c == ':' && frameLength_ > 0) ? PAYLOAD : SCANNING;
                }
                break;
            case PAYLOAD:
                feedHttp(c);
                if (--frameLength_ == 0) frame_ = SCANNING;
                break;
        }
    }

    bool closed() const { return closed_; }

    // The body is only trustworthy once the server has closed the connection.
    bool complete() const { return closed_ && http_ == BODY; }

    // HTTP status code, or 0 if no complete status line arrived.
    int status() const { return statusDigits_ == 3 ? status_ : 0; }

    // Replies from the device endpoints are short ("OK", "0", "1"); anything longer is cut.
    const char *body() const { return body_; }

private:
    enum FrameState : uint8_t { SCANNING, FRAME_LENGTH, PAYLOAD };
    enum HttpState : uint8_t { STATUS_VERSION, STATUS_CODE, HEADERS, BODY };

    static bool matches(const char *token, uint8_t &position, char c) {
        position = (c == token[position]) ? position + 1 : (c == token[0] ? 1 : 0);
        if (token[position] != '\0') return false;
        position = 0;
        return true;
    }

    void feedHttp(char c) {
        switch (http_) {
            case STATUS_VERSION:
                if (c == ' ') http_ = STATUS_CODE;
                break;
            case STATUS_CODE:
                if (c >= '0' && c <= '9' && statusDigits_ < 3) {
                    status_ = status_ * 10 + (c - '0');
                    statusDigits_++;
                } else {
                    http_ = HEADERS;
                }
                break;
            case HEADERS:
                if (matches("\r\n\r\n", headerEndMatch_, c)) http_ = BODY;
                break;
            case BODY:
                if (bodyLength_ < MAX_BODY) {
                    body_[bodyLength_++] = c;
                    body_[bodyLength_] = '\0';
                }
                break;
        }
    }

    static const uint8_t MAX_BODY = 7;

    FrameState frame_;
    uint8_t ipdMatch_, closedMatch_;
    uint16_t frameLength_;
    HttpState http_;
    int status_;
    uint8_t statusDigits_;
    uint8_t headerEndMatch_;
    uint8_t bodyLength_;
    char body_[MAX_BODY + 1];
    bool closed_;
};
