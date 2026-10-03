#include <stdio.h>
#include <string.h>
#include "client.h"
#include "server.h"
#include <sys/stat.h>
#include <fcntl.h>
 #include <unistd.h>

void prepare_response(struct client *c, int code, const char *body) {
    const char *status_line;
    switch (code) {
        case 200:
            status_line = "HTTP/1.1 200 OK";
            break;
        case 404:
            status_line = "HTTP/1.1 404 Not Found";     
            break;
        case 429:
            status_line = "HTTP/1.1 429 Too Many Requests";
            break;
        case 413:
            status_line = "HTTP/1.1 413 Payload Too Large";
            break;
        case 500:
            status_line = "HTTP/1.1 500 Internal Server Error";
            break;
        default:
            status_line = "HTTP/1.1 400 Bad Request";
            break;
    }
    const char *conn = c->keep_alive ? "keep-alive" : "close";
    
    int len = snprintf(c->buffer_out, sizeof(c->buffer_out),
        "%s\r\n"
        "Content-Length: %zu\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: %s\r\n"
        "\r\n"
        "%s",
        status_line, strlen(body),conn, body);

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
    if(strstr(path, ".pdf")) return "application/pdf";
    return "application/octet-stream";
}

int prepare_file_response(struct client *c, const char *file_path) {
    int fd = open(file_path, O_RDONLY);
    if (fd<0) {
        return -1; 
    }

    struct stat st;
    if (fstat(fd, &st) < 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        return -1;
    }
    const char *conn = c->keep_alive ? "keep-alive" : "close";

    int header_len = snprintf(c->buffer_out, sizeof(c->buffer_out),
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: %lld\r\n"
        "Content-Type: %s\r\n"
        "Connection: %s\r\n\r\n",
        (long long)st.st_size, get_content_type(file_path),conn);


    c->out_len = header_len;
    c->out_sent = 0;
    c->resp_file = fd;
    c->file_remaining = (size_t)st.st_size;

    return 0; // Sucesso
}
