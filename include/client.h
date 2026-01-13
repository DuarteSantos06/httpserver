#pragma once
#include "server.h"

enum client_state {
    C_READING,
    C_WRITING,
    C_CLOSED
};

struct client{
    int fd;
    enum client_state state;

    char buffer_in[BUF_SIZE];
    int in_len;

    char buffer_out[BUF_SIZE];
    int out_len;
    int out_sent;
};