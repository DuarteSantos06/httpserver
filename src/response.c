#include <stdio.h>
#include <string.h>
#include "client.h"

void prepare_response(struct client *c, int code, const char *body) {
    const char *status_line;
    switch (code) {
        case 200:
            status_line = "HTTP/1.1 200 OK";
            break;
        case 404:
            status_line = "HTTP/1.1 404 Not Found";     
            break;
        default:
            status_line = "HTTP/1.1 400 Bad Request";
            break;
    }

    int len = snprintf(c->buffer_out, sizeof(c->buffer_out),
        "%s\r\n"
        "Content-Length: %zu\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        status_line, strlen(body), body);

    c->out_len = len;
    c->out_sent = 0;
}

void prepare_status_response(struct client *c) {
    char json[512];

    unsigned long total = atomic_load(&g_requests_total);
    unsigned long open  = atomic_load(&g_connections_open);

    snprintf(json, sizeof(json),
        "{"
            "\"status\": \"ok\","
            "\"requests_total\": %lu,"
            "\"connections_open\": %lu"
        "}",
        total,
        open
    );
    prepare_response(c, 200, json);
}