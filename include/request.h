#pragma once

struct request{
    char method[8];
    char path[1024];
    char http_version[16];
};