#pragma once
#include <time.h>
#include <netinet/in.h>
#include "uthash.h"
#include "server.h"

#define MAX_TOKENS 5.0
#define RATE 5.0
#define CLEANUP_INTERVAL 60

typedef struct {
    char ip[INET_ADDRSTRLEN];
    double tokens;        
    double last_time;     
    UT_hash_handle hh;
}ip_entry;

extern ip_entry* ip_map;


void *cleanup_ip_table(void* arg);
void addClientIpToTable(char *ip);
int isRateLimited(struct in_addr addr);