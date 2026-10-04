#include <string.h>
#include "test.h"
#include "http.h"
#include "request.h"
#include "server.h"

// parse_request writes into the caller's buffer only via sscanf/strstr, but
// the signature takes char*, so copy literals into a writable buffer.
static int parse(const char *raw, struct request *req)
{
    static char buf[16384];
    snprintf(buf, sizeof(buf), "%s", raw);
    // Fill with garbage so fields the parser forgets to set are noticed
    memset(req, 0x7f, sizeof(*req));
    return parse_request(buf, req);
}

static void test_parse_simple_get(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("GET /index.html HTTP/1.1\r\nHost: x\r\n\r\n", &req), 0);
    EXPECT_EQ_STR(req.method, "GET");
    EXPECT_EQ_STR(req.path, "/index.html");
    EXPECT_EQ_STR(req.http_version, "HTTP/1.1");
    EXPECT_EQ_INT(req.content_length, 0);
    EXPECT_EQ_STR(req.body, "");
}

static void test_parse_incomplete_headers_fails(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("GET / HTTP/1.1\r\nHost: x\r\n", &req), -1);
}

static void test_parse_malformed_request_line_fails(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("GET\r\n\r\n", &req), -1);
}

static void test_parse_post_body(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("POST /data HTTP/1.1\r\nContent-Length: 5\r\n\r\nhello", &req), 0);
    EXPECT_EQ_STR(req.method, "POST");
    EXPECT_EQ_INT(req.content_length, 5);
    EXPECT_EQ_STR(req.body, "hello");
}

static void test_parse_body_shorter_than_buffer(void)
{
    struct request req;
    // Only Content-Length bytes are copied into body
    EXPECT_EQ_INT(parse("POST /data HTTP/1.1\r\nContent-Length: 3\r\n\r\nhello", &req), 0);
    EXPECT_EQ_STR(req.body, "hel");
}

static void test_parse_negative_content_length_fails(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("POST /data HTTP/1.1\r\nContent-Length: -1\r\n\r\n", &req), -1);
}

static void test_parse_too_large_content_length_fails(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("POST /data HTTP/1.1\r\nContent-Length: 8192\r\n\r\n", &req), -1);
}

static void test_parse_invalid_content_length_fails(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("POST /data HTTP/1.1\r\nContent-Length: abc\r\n\r\n", &req), -1);
}

static void test_parse_content_length_in_body_is_ignored(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("GET / HTTP/1.1\r\n\r\nContent-Length: 5", &req), 0);
    EXPECT_EQ_INT(req.content_length, 0);
}

static void test_parse_http11_defaults_to_keep_alive(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("GET / HTTP/1.1\r\n\r\n", &req), 0);
    EXPECT_EQ_INT(req.keep_alive, 1);
}

static void test_parse_http11_connection_close(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("GET / HTTP/1.1\r\nConnection: close\r\n\r\n", &req), 0);
    EXPECT_EQ_INT(req.keep_alive, 0);
}

static void test_parse_http10_defaults_to_close(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("GET / HTTP/1.0\r\n\r\n", &req), 0);
    EXPECT_EQ_INT(req.keep_alive, 0);
}

static void test_parse_http10_connection_keep_alive(void)
{
    struct request req;
    EXPECT_EQ_INT(parse("GET / HTTP/1.0\r\nConnection: Keep-Alive\r\n\r\n", &req), 0);
    EXPECT_EQ_INT(req.keep_alive, 1);
}

static void test_parse_counts_requests(void)
{
    struct request req;
    unsigned long before = g_requests_total;
    parse("GET / HTTP/1.1\r\n\r\n", &req);
    EXPECT_EQ_INT(g_requests_total, before + 1);
    parse("garbage", &req);
    EXPECT_EQ_INT(g_requests_total, before + 1);
}

void run_http_tests(void)
{
    printf("-- http.c\n");
    RUN_TEST(test_parse_simple_get);
    RUN_TEST(test_parse_incomplete_headers_fails);
    RUN_TEST(test_parse_malformed_request_line_fails);
    RUN_TEST(test_parse_post_body);
    RUN_TEST(test_parse_body_shorter_than_buffer);
    RUN_TEST(test_parse_negative_content_length_fails);
    RUN_TEST(test_parse_too_large_content_length_fails);
    RUN_TEST(test_parse_invalid_content_length_fails);
    RUN_TEST(test_parse_content_length_in_body_is_ignored);
    RUN_TEST(test_parse_http11_defaults_to_keep_alive);
    RUN_TEST(test_parse_http11_connection_close);
    RUN_TEST(test_parse_http10_defaults_to_close);
    RUN_TEST(test_parse_http10_connection_keep_alive);
    RUN_TEST(test_parse_counts_requests);
}
