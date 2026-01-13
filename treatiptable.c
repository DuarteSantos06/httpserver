#include "server.h"
#include <unistd.h>
#include "treatiptable.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>


ip_entry* ip_map = NULL;
pthread_mutex_t ip_table_mutex = PTHREAD_MUTEX_INITIALIZER;


void addClientIpToTable(char *ip){
    pthread_mutex_lock(&ip_table_mutex);
    ip_entry *entry;
    HASH_FIND_STR(ip_map, ip, entry);
    
    if (entry == NULL) {
        entry=malloc(sizeof(ip_entry));
        entry->last_time=time(NULL);
        entry->tokens=MAX_TOKENS;
        strncpy(entry->ip, ip, INET_ADDRSTRLEN);
        HASH_ADD_STR(ip_map, ip, entry);
    }else{
        entry->last_time=time(NULL);
    }
    pthread_mutex_unlock(&ip_table_mutex);
}

int isRateLimited(struct in_addr addr){
    pthread_mutex_lock(&ip_table_mutex);
    char ip[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr, ip, sizeof(ip));
    ip_entry *entry;
    HASH_FIND_STR(ip_map, ip, entry);

    if (entry != NULL) {
        double now = time(NULL);             
        double elapsed = now - entry->last_time;
        entry->tokens += elapsed * RATE;
        if(entry->tokens > MAX_TOKENS) {
            entry->tokens = MAX_TOKENS;
        }
        if(entry->tokens < 1.0) {
            pthread_mutex_unlock(&ip_table_mutex);
            return 1; 
        } else {
            entry->tokens -= 1.0; 
            entry->last_time = now; 
            pthread_mutex_unlock(&ip_table_mutex);
            return 0; 
        }
    }
    pthread_mutex_unlock(&ip_table_mutex);
    return 0;
}

