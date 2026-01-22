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
    size_t in_len;

    size_t header_len;
    size_t body_expected;

    char buffer_out[BUF_SIZE];
    size_t out_len;
    size_t out_sent;
};