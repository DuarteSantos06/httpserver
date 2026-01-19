#include <stdio.h>
#include <string.h>
#include "client.h"
#include "request.h"

int parse_request(char* buffer,struct request *req){
    char* end=strstr(buffer,"\r\n\r\n");
    if(!end)return -1;
    if (sscanf(buffer, "%7s %1023s %15s", req->method, req->path, req->http_version) != 3)
        return -1;
    g_requests_total++;
    return 0;    
}