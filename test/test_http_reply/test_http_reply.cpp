// Run with: pio test -e native
#include <stdio.h>
#include <string.h>
#include <unity.h>

#include "http_reply.h"

static const char HEADERS[] =
    "HTTP/1.1 200 OK\r\n"
    "date: Tue, 03 Nov 2026 14:00:00 GMT\r\n"
    "server: uvicorn\r\n"
    "content-length: 1\r\n"
    "content-type: text/plain; charset=utf-8\r\n"
    "connection: close\r\n\r\n";

static void feedAll(HttpReply &reply, const char *text) {
    while (*text) reply.feed(*text++);
}

static void feedFrame(HttpReply &reply, const char *payload) {
    char prefix[20];
    snprintf(prefix, sizeof(prefix), "\r\n+IPD,%u:", (unsigned)strlen(payload));
    feedAll(reply, prefix);
    feedAll(reply, payload);
}

void setUp() {}
void tearDown() {}

void test_headers_and_body_in_separate_frames() {
    HttpReply reply;
    feedAll(reply, "\r\nRecv 90 bytes\r\n\r\nSEND OK\r\n");
    feedFrame(reply, HEADERS);
    feedFrame(reply, "1");
    feedAll(reply, "CLOSED\r\n");

    TEST_ASSERT_TRUE(reply.complete());
    TEST_ASSERT_EQUAL_INT(200, reply.status());
    TEST_ASSERT_EQUAL_STRING("1", reply.body());
}

void test_reply_arriving_before_send_ok() {
    HttpReply reply;
    feedFrame(reply, HEADERS);
    feedFrame(reply, "0");
    feedAll(reply, "\r\nSEND OK\r\n\r\nCLOSED\r\n");

    TEST_ASSERT_TRUE(reply.complete());
    TEST_ASSERT_EQUAL_STRING("0", reply.body());
}

void test_frame_split_inside_header_terminator() {
    HttpReply reply;
    feedFrame(reply, "HTTP/1.1 200 OK\r\ncontent-length: 2\r\n\r");
    feedFrame(reply, "\n\r\nOK");
    feedAll(reply, "CLOSED\r\n");

    // "\r\n\r" then "\n" joins up across the frame boundary; the second "\r\n" is body.
    TEST_ASSERT_TRUE(reply.complete());
    TEST_ASSERT_EQUAL_INT(200, reply.status());
}

void test_error_status_is_reported() {
    HttpReply reply;
    feedFrame(reply, "HTTP/1.1 422 Unprocessable Content\r\ncontent-length: 2\r\n\r\n{}");
    feedAll(reply, "CLOSED\r\n");

    TEST_ASSERT_TRUE(reply.complete());
    TEST_ASSERT_EQUAL_INT(422, reply.status());
}

void test_closed_inside_the_data_does_not_end_the_reply() {
    HttpReply reply;
    feedFrame(reply, "HTTP/1.1 200 OK\r\nx-note: CLOSED\r\n\r\nOK");

    TEST_ASSERT_FALSE(reply.closed());
    TEST_ASSERT_EQUAL_STRING("OK", reply.body());
}

void test_cut_off_before_the_body_is_incomplete() {
    HttpReply reply;
    feedFrame(reply, "HTTP/1.1 200 OK\r\ncontent-");
    feedAll(reply, "CLOSED\r\n");

    TEST_ASSERT_TRUE(reply.closed());
    TEST_ASSERT_FALSE(reply.complete());
}

void test_connection_failure_has_no_status() {
    HttpReply reply;
    feedAll(reply, "\r\nSEND FAIL\r\nCLOSED\r\n");

    TEST_ASSERT_FALSE(reply.complete());
    TEST_ASSERT_EQUAL_INT(0, reply.status());
}

void test_long_body_is_cut_without_overflow() {
    HttpReply reply;
    feedFrame(reply, "HTTP/1.1 200 OK\r\n\r\n0123456789abcdef0123456789");
    feedAll(reply, "CLOSED\r\n");

    TEST_ASSERT_TRUE(reply.complete());
    TEST_ASSERT_EQUAL_STRING("0123456", reply.body());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_headers_and_body_in_separate_frames);
    RUN_TEST(test_reply_arriving_before_send_ok);
    RUN_TEST(test_frame_split_inside_header_terminator);
    RUN_TEST(test_error_status_is_reported);
    RUN_TEST(test_closed_inside_the_data_does_not_end_the_reply);
    RUN_TEST(test_cut_off_before_the_body_is_incomplete);
    RUN_TEST(test_connection_failure_has_no_status);
    RUN_TEST(test_long_body_is_cut_without_overflow);
    return UNITY_END();
}
