#include "server.h"
#include <unistd.h>
#include "treatiptable.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>
#include <pthread.h>
#include "stdio.h"


ip_entry* ip_map [MAX_SHARDS] = {NULL};
pthread_mutex_t shard_mutexes[MAX_SHARDS]={PTHREAD_MUTEX_INITIALIZER};


static unsigned long hash(const char *str) {
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; 
    }
    return hash % MAX_SHARDS;
}

void addClientIpToTable(char *ip){
    int shard=hash(ip);
    pthread_mutex_lock(&shard_mutexes[shard]);
    
    ip_entry *entry;
    HASH_FIND_STR(ip_map[shard], ip, entry);
    
    if (entry == NULL) {
        entry=malloc(sizeof(ip_entry));
        entry->last_time=time(NULL);
        entry->tokens=MAX_TOKENS;
        strncpy(entry->ip, ip, IP_STR_LEN-1);
        HASH_ADD_STR(ip_map[shard], ip, entry);
    }else{
        entry->last_time=time(NULL);
    }
    pthread_mutex_unlock(&shard_mutexes[shard]);
}

int isRateLimited(const char *ip){

    int shard = hash(ip);
    pthread_mutex_lock(&shard_mutexes[shard]);
    ip_entry *entry;
    HASH_FIND_STR(ip_map[shard], ip, entry);
    if (entry != NULL) {
        double now = time(NULL);             
        double elapsed = now - entry->last_time;
        entry->tokens += elapsed * RATE;
        if(entry->tokens > MAX_TOKENS) {
            entry->tokens = MAX_TOKENS;
        }
        if(entry->tokens < 1.0) {
            pthread_mutex_unlock(&shard_mutexes[shard]);
            return 1; 
        } else {
            entry->tokens -= 1.0; 
            entry->last_time = now; 
            pthread_mutex_unlock(&shard_mutexes[shard]);
            return 0; 
        }
    }
    pthread_mutex_unlock(&shard_mutexes[shard]);
    return 0;
}

void *cleanup_ip_table(void* arg){
    (void )arg;
    while(1)
    {
        printf("Cleaning up IP table...\n");
        ip_entry *current_entry, *tmp;
        for(int shard=0;shard<MAX_SHARDS;shard++ ){
            pthread_mutex_lock(&shard_mutexes[shard]);
            HASH_ITER(hh, ip_map[shard], current_entry, tmp) {
                if (time(NULL) - current_entry->last_time > CLEANUP_INTERVAL) {
                    HASH_DEL(ip_map[shard], current_entry);
                    free(current_entry);
                }
            }
            pthread_mutex_unlock(&shard_mutexes[shard]);
        }
        sleep(CLEANUP_INTERVAL);
    }
    return NULL;
}

