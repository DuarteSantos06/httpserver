#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "test.h"
#include "client.h"
#include "handle_http_request.h"

// Tests handle_http_request together with the handlers in handlers_utils.c.
// Runs inside the scratch directory created by main.c (www/, data/, audit/).

static void send_request(struct client *c, const char *raw)
{
    memset(c, 0, sizeof(*c));
    c->resp_file = -1;
    strcpy(c->ip, "127.0.0.1");
    snprintf(c->buffer_in, sizeof(c->buffer_in), "%s", raw);
    c->in_len = strlen(c->buffer_in);
    handle_http_request(c);
}

static void close_resp_file(struct client *c)
{
    if (c->resp_file >= 0)
        close(c->resp_file);
    c->resp_file = -1;
}

static void write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "w");
    fputs(content, f);
    fclose(f);
}

static void test_route_status(void)
{
    struct client c;
    send_request(&c, "GET /status HTTP/1.1\r\n\r\n");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 200 OK\r\n");
    EXPECT_CONTAINS(c.buffer_out, "\"status\": \"ok\"");
}

static void test_bad_request(void)
{
    struct client c;
    send_request(&c, "NOT HTTP\r\n");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 400 Bad Request\r\n");
}

static void test_keep_alive_copied_to_client(void)
{
    struct client c;
    send_request(&c, "GET /status HTTP/1.1\r\nConnection: close\r\n\r\n");
    EXPECT_EQ_INT(c.keep_alive, 0);
    EXPECT_CONTAINS(c.buffer_out, "Connection: close\r\n");

    send_request(&c, "GET /status HTTP/1.1\r\n\r\n");
    EXPECT_EQ_INT(c.keep_alive, 1);
    EXPECT_CONTAINS(c.buffer_out, "Connection: keep-alive\r\n");
}

static void test_unknown_method_not_found(void)
{
    struct client c;
    send_request(&c, "DELETE /status HTTP/1.1\r\n\r\n");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 404 Not Found\r\n");
}

static void test_post_unknown_path_not_found(void)
{
    struct client c;
    send_request(&c, "POST /other HTTP/1.1\r\nContent-Length: 1\r\n\r\nx");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 404 Not Found\r\n");
}

static void test_post_data_stores_body(void)
{
    unlink("data/data.txt");
    struct client c;
    send_request(&c, "POST /data HTTP/1.1\r\nContent-Length: 6\r\n\r\nfirst\n");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 200 OK\r\n");
    send_request(&c, "POST /data HTTP/1.1\r\nContent-Length: 7\r\n\r\nsecond\n");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 200 OK\r\n");

    char buf[64] = {0};
    FILE *f = fopen("data/data.txt", "r");
    EXPECT(f != NULL);
    if (!f) return;
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    EXPECT_EQ_STR(buf, "first\nsecond\n");
}

static void test_post_data_without_body(void)
{
    struct client c;
    send_request(&c, "POST /data HTTP/1.1\r\n\r\n");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 400 Bad Request\r\n");
    EXPECT_CONTAINS(c.buffer_out, "No body");
}

static void test_static_root_serves_index(void)
{
    write_file("www/index.html", "<h1>hi</h1>");
    struct client c;
    send_request(&c, "GET / HTTP/1.1\r\n\r\n");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 200 OK\r\n");
    EXPECT_CONTAINS(c.buffer_out, "Content-Type: text/html; charset=UTF-8\r\n");
    EXPECT_CONTAINS(c.buffer_out, "Content-Length: 11\r\n");
    EXPECT(c.resp_file >= 0);
    close_resp_file(&c);
}

static void test_static_file(void)
{
    write_file("www/app.js", "x()");
    struct client c;
    send_request(&c, "GET /app.js HTTP/1.1\r\n\r\n");
    EXPECT_CONTAINS(c.buffer_out, "Content-Type: application/javascript\r\n");
    EXPECT_EQ_INT(c.file_remaining, 3);
    close_resp_file(&c);
}

static void test_static_missing_file(void)
{
    struct client c;
    send_request(&c, "GET /missing.html HTTP/1.1\r\n\r\n");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 404 Not Found\r\n");
    EXPECT_EQ_INT(c.resp_file, -1);
}

static void test_path_traversal_rejected_and_audited(void)
{
    unlink("audit/audit.log");
    struct client c;
    send_request(&c, "GET /../secret HTTP/1.1\r\n\r\n");
    EXPECT_CONTAINS(c.buffer_out, "HTTP/1.1 400 Bad Request\r\n");
    EXPECT_CONTAINS(c.buffer_out, "Invalid Path");
    EXPECT_EQ_INT(c.resp_file, -1);

    char line[256] = {0};
    FILE *f = fopen("audit/audit.log", "r");
    EXPECT(f != NULL);
    if (!f) return;
    EXPECT(fgets(line, sizeof(line), f) != NULL);
    fclose(f);
    EXPECT_CONTAINS(line, "PATH TRAVERSAL ATTEMPT");
    EXPECT_CONTAINS(line, "IP: 127.0.0.1");
    EXPECT_CONTAINS(line, "/../secret");
}

void run_router_tests(void)
{
    printf("-- handle_http_request.c / handlers_utils.c / audit.c\n");
    RUN_TEST(test_route_status);
    RUN_TEST(test_bad_request);
    RUN_TEST(test_keep_alive_copied_to_client);
    RUN_TEST(test_unknown_method_not_found);
    RUN_TEST(test_post_unknown_path_not_found);
    RUN_TEST(test_post_data_stores_body);
    RUN_TEST(test_post_data_without_body);
    RUN_TEST(test_static_root_serves_index);
    RUN_TEST(test_static_file);
    RUN_TEST(test_static_missing_file);
    RUN_TEST(test_path_traversal_rejected_and_audited);
}
