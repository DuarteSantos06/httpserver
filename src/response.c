#include <stdio.h>
#include <string.h>
#include "client.h"
#include "server.h"

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

void prepare_429_response(struct client *c) {
    const char *body = "Too Many Requests\n";
    prepare_response(c, 429, body);
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

const char* get_content_type(const char *path) {
    if (strstr(path, ".html")) return "text/html; charset=UTF-8";
    if (strstr(path, ".css"))  return "text/css";
    if (strstr(path, ".js"))   return "application/javascript";
    if (strstr(path, ".png"))  return "image/png";
    if (strstr(path, ".jpg") || strstr(path, ".jpeg")) return "image/jpeg";
    return "application/octet-stream";
}

int prepare_file_response(struct client *c, const char *file_path) {
    FILE *file = fopen(file_path, "rb");
    if (!file) {
        return -1; 
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    int header_len = snprintf(c->buffer_out, sizeof(c->buffer_out),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %ld\r\n"
        "Content-Type: %s\r\n"
        "Connection: close\r\n\r\n",
        file_size, get_content_type(file_path));

    size_t bytes_read = fread(c->buffer_out + header_len, 1, sizeof(c->buffer_out) - header_len, file);
    fclose(file);

    c->out_len = header_len + bytes_read;
    c->out_sent = 0;

    return 0; // Sucesso
}
