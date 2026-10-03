#pragma once
#include <stddef.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>


#define BUF_SIZE 8192

enum client_state {
    C_READING,
    C_WRITING,
    C_CLOSED
};

struct client{
    int fd;
    char ip[INET6_ADDRSTRLEN];
    enum client_state state;

    char buffer_in[BUF_SIZE];
    size_t in_len;

    size_t header_len;
    size_t body_expected;

    char buffer_out[BUF_SIZE];
    size_t out_len;
    size_t out_sent;

    int resp_file;
    size_t file_remaining;
    
    off_t file_offset;

    int keep_alive;

    time_t last_activity;

    struct client *prev;
    struct client *next;
};