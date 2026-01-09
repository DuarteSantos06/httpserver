#pragma once

#define SERVER_PORT 8080
#define WORKER_COUNT 8

#define MAX_CLIENTS 65536
#define BUF_SIZE 8192
#define MAX_REQUESTS 5

extern volatile int running; 

