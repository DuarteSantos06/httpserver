#include <stdio.h>
#include <string.h>
#include "client.h"

void prepare_response(struct client *c, int code, const char *body) {
    const char *status_line;
    if (code == 200) status_line = "HTTP/1.1 200 OK";
    else if (code == 404) status_line = "HTTP/1.1 404 Not Found";
    else status_line = "HTTP/1.1 400 Bad Request";

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