#include <stdio.h>
#include <string.h>
#include "client.h"
#include "request.h"
#include "constants.h"


int parse_request(char* buffer,struct request *req){
    char* end=strstr(buffer,"\r\n\r\n");
    if(!end)return -1;
    if (sscanf(buffer, "%7s %1023s %15s", req->method, req->path, req->http_version) != 3)
        return -1;

    req->content_length=0;
    req->body[0]='\0';
    size_t headers_len = end - buffer;
    char *cl=strstr(buffer,"Content-Length:");
    if(cl && (size_t)(cl - buffer) < headers_len){
        if (sscanf(cl, "Content-Length: %d", &req->content_length) != 1)
            return -1;

        if (req->content_length < 0 || req->content_length >= MAX_BODY)
            return -1;
        char *body_start=end+4;
        memcpy(req->body, body_start, req->content_length);
        req->body[req->content_length] = '\0';
    }
    g_requests_total++;
    return 0;   
    
}