#pragma once
#include <time.h>
#include <netinet/in.h>
#include "uthash.h"
#include "server.h"

#define MAX_TOKENS 5
#define RATE 5
#define CLEANUP_INTERVAL 60
#define MAX_SHARDS 256
#define IP_STR_LEN INET6_ADDRSTRLEN

typedef struct {
    char ip[IP_STR_LEN];
    double tokens;        
    double last_time;     
    UT_hash_handle hh;
}ip_entry;

extern ip_entry* ip_map[MAX_SHARDS];
extern pthread_mutex_t shard_mutexes[MAX_SHARDS];



void *cleanup_ip_table(void* arg);
void addClientIpToTable(char *ip);
int isRateLimited(const char *ip);