#include "server.h"
#include <unistd.h>
#include "treatiptable.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>


ip_entry* ip_map = NULL;


void addClientIpToTable(char *ip){
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
    
}

int isRateLimited(struct in_addr addr){
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
            return 1; 
        } else {
            entry->tokens -= 1.0; 
            entry->last_time = now; 
            return 0; 
        }
    }
    return 0;
}

