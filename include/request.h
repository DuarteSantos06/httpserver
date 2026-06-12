#pragma once

#include <stdio.h>
#include "handle_http_request.h"


struct request{
    char method[8];
    char path[1024];
    char http_version[16];
    int content_length;
    char body[MAX_BODY];
};