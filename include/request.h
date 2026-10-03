#pragma once

#include <stdio.h>

#define MAX_BODY 8192


struct request{
    char method[8];
    char path[1024];
    char http_version[16];
    int content_length;
    char body[MAX_BODY];
    int keep_alive;
};