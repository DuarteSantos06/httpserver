#include <stdio.h>
#include <string.h>
#include "client.h"

int parse_request(char* buffer,char* path){
    char* end=strstr(buffer,"\r\n\r\n");
    if(!end)return -1;
    char method[8];
    if (sscanf(buffer, "%7s %1023s", method, path) != 2)
        return -1;

    return 0;    
}