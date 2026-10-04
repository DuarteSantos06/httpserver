#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "test.h"
#include "client.h"
#include "response.h"
#include "server.h"

// Not declared in response.h
const char *get_content_type(const char *path);

static void reset_client(struct client *c)
{
    memset(c, 0, sizeof(*c));
    c->resp_file = -1;
}

static void test_response_200(void)
{
    struct client c;
    reset_client(&c);
    c.keep_alive = 1;
    c.out_sent = 42;
    prepare_response(&c, 200, "hello");

    const char *expected =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 5\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: keep-alive\r\n"
        "\r\n"
        "hello";
    EXPECT_EQ_STR(c.buffer_out, expected);
    EXPECT_EQ_INT(c.out_len, strlen(expected));
    EXPECT_EQ_INT(c.out_sent, 0);
}

static void test_response_connection_close(void)
{
    struct client c;
    reset_client(&c);
    c.keep_alive = 0;
    prepare_response(&c, 200, "");
    EXPECT_CONTAINS(c.buffer_out, "Connection: close\r\n");
    EXPECT_CONTAINS(c.buffer_out, "Content-Length: 0\r\n");
}

static void test_response_status_lines(void)
{
    struct { int code; const char *line; } cases[] = {
        { 404, "HTTP/1.1 404 Not Found\r\n" },
        { 413, "HTTP/1.1 413 Payload Too Large\r\n" },
        { 429, "HTTP/1.1 429 Too Many Requests\r\n" },
        { 500, "HTTP/1.1 500 Internal Server Error\r\n" },
        { 400, "HTTP/1.1 400 Bad Request\r\n" },
        { 999, "HTTP/1.1 400 Bad Request\r\n" },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        struct client c;
        reset_client(&c);
        prepare_response(&c, cases[i].code, "x");
        EXPECT(strncmp(c.buffer_out, cases[i].line, strlen(cases[i].line)) == 0);
    }
}

static void test_response_429(void)
{
    struct client c;
    reset_client(&c);
    prepare_429_response(&c);
    EXPECT_CONTAINS(c.buffer_out, "429 Too Many Requests");
    EXPECT_CONTAINS(c.buffer_out, "\r\n\r\nToo Many Requests\n");
}

static void test_status_response_reports_counters(void)
{
    struct client c;
    reset_client(&c);
    unsigned long saved_total = g_requests_total;
    unsigned long saved_open = g_connections_open;
    g_requests_total = 123;
    g_connections_open = 7;

    prepare_status_response(&c);
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 200 OK\r\n");
    EXPECT_CONTAINS(c.buffer_out, "\"status\": \"ok\"");
    EXPECT_CONTAINS(c.buffer_out, "\"requests_total\": 123");
    EXPECT_CONTAINS(c.buffer_out, "\"connections_open\": 7");

    g_requests_total = saved_total;
    g_connections_open = saved_open;
}

static void test_content_types(void)
{
    EXPECT_EQ_STR(get_content_type("www/index.html"), "text/html; charset=UTF-8");
    EXPECT_EQ_STR(get_content_type("a.css"), "text/css");
    EXPECT_EQ_STR(get_content_type("a.js"), "application/javascript");
    EXPECT_EQ_STR(get_content_type("a.png"), "image/png");
    EXPECT_EQ_STR(get_content_type("a.jpg"), "image/jpeg");
    EXPECT_EQ_STR(get_content_type("a.jpeg"), "image/jpeg");
    EXPECT_EQ_STR(get_content_type("a.pdf"), "application/pdf");
    EXPECT_EQ_STR(get_content_type("a.bin"), "application/octet-stream");
}

static void test_file_response_existing_file(void)
{
    FILE *f = fopen("www/test_file.css", "w");
    fputs("body{}", f);
    fclose(f);

    struct client c;
    reset_client(&c);
    c.keep_alive = 1;
    EXPECT_EQ_INT(prepare_file_response(&c, "www/test_file.css"), 0);

    const char *expected =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 6\r\n"
        "Content-Type: text/css\r\n"
        "Connection: keep-alive\r\n\r\n";
    EXPECT_EQ_STR(c.buffer_out, expected);
    EXPECT_EQ_INT(c.out_len, strlen(expected));
    EXPECT_EQ_INT(c.file_remaining, 6);
    EXPECT(c.resp_file >= 0);
    if (c.resp_file >= 0)
        close(c.resp_file);
}

static void test_file_response_missing_file(void)
{
    struct client c;
    reset_client(&c);
    EXPECT_EQ_INT(prepare_file_response(&c, "www/does_not_exist"), -1);
    EXPECT_EQ_INT(c.resp_file, -1);
}

static void test_file_response_directory_rejected(void)
{
    struct client c;
    reset_client(&c);
    EXPECT_EQ_INT(prepare_file_response(&c, "www"), -1);
    EXPECT_EQ_INT(c.resp_file, -1);
}

void run_response_tests(void)
{
    printf("-- response.c\n");
    RUN_TEST(test_response_200);
    RUN_TEST(test_response_connection_close);
    RUN_TEST(test_response_status_lines);
    RUN_TEST(test_response_429);
    RUN_TEST(test_status_response_reports_counters);
    RUN_TEST(test_content_types);
    RUN_TEST(test_file_response_existing_file);
    RUN_TEST(test_file_response_missing_file);
    RUN_TEST(test_file_response_directory_rejected);
}
