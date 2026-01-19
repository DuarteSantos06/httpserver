#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#include "loop.h"
#include "server.h"
#include "socket.h"
#include "client.h"
#include "treatiptable.h"



volatile int running = 1;
atomic_ulong g_requests_total = 0;
atomic_ulong g_connections_open = 0;


int main() {
    int server_fd=server_socket(SERVER_PORT);

    printf("Servidor a ouvir na porta 8080...\n");
    pthread_t clean_ip_table_thread;
    if(server_fd<0){
        perror("server_socket");
        return 1;
    }

    pthread_create(&clean_ip_table_thread, NULL, cleanup_ip_table, NULL);
    pthread_detach(clean_ip_table_thread);

    pthread_t workers[WORKER_COUNT];

    for(int i=0;i<WORKER_COUNT;i++)
    {
        if (pthread_create(&workers[i], NULL, worker_loop, (void*)(intptr_t)server_fd) != 0) {
            perror("pthread_create");
            return 1;
        }
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }

    close(server_fd);
    return 0;


}