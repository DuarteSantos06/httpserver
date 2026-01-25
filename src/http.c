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
    char *cl=strstr(buffer,"Content-Length:");
    if(cl){
        sscanf(cl,"Content-Length: %d",&req->content_length);
        if(req->content_length>MAX_BODY){
            return -1;
        }
        char *body_start=end+4;
        if((size_t)req->content_length <= strlen(body_start)){
            strncpy(req->body,body_start,req->content_length);
            req->body[req->content_length]='\0';
        }else{
            return -1;
        }
    }    
    g_requests_total++;
    return 0;    
}