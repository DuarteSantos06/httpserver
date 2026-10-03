#include <stdio.h>
#include <string.h>
#include "client.h"
#include "request.h"
#include "server.h"


int parse_request(char* buffer,struct request *req){
    char* end=strstr(buffer,"\r\n\r\n");
    if(!end)return -1;
    if (sscanf(buffer, "%7s %1023s %15s", req->method, req->path, req->http_version) != 3)
        return -1;

    req->content_length=0;
    req->body[0]='\0';
    size_t headers_len = end - buffer;
    char *content_length=strstr(buffer,"Content-Length:");
    char *connection=strstr(buffer,"Connection:");
    if(content_length && (size_t)(content_length - buffer) < headers_len){
        if (sscanf(content_length, "Content-Length: %d", &req->content_length) != 1)
            return -1;

        if (req->content_length < 0 || req->content_length >= MAX_BODY)
            return -1;
        char *body_start=end+4;
        memcpy(req->body, body_start, req->content_length);
        req->body[req->content_length] = '\0';
    }
    if (strcmp(req->http_version, "HTTP/1.1") == 0)
    {
        req->keep_alive = 1;
    }
    char conn_value[32];
    if( connection && (size_t)(connection - buffer) < headers_len){
        if (sscanf(connection, "Connection: %31[^\r\n]", conn_value) == 1) {
        if (strcasecmp(conn_value, "keep-alive") == 0) {
            req->keep_alive = 1;
        } else {
            req->keep_alive = 0;
        }
        } else {
            req->keep_alive = 0; 
        }
    }
    g_requests_total++;
    return 0;   
    
}