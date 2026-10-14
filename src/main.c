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



atomic_bool running = 1;
atomic_ulong g_requests_total = 0;
atomic_ulong g_connections_open = 0;

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        running = 0;
    }
}

int main() {
    // One listening socket per worker, all bound to the same port with
    // SO_REUSEPORT, so the kernel spreads new connections evenly across workers
    int server_fds[WORKER_COUNT];
    for(int i=0;i<WORKER_COUNT;i++)
    {
        server_fds[i]=server_socket(SERVER_PORT);
        if(server_fds[i]<0){
            perror("server_socket");
            return 1;
        }
    }

    printf("Server listening on port 8080...\n");
    pthread_t clean_ip_table_thread;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    // sendfile() has no MSG_NOSIGNAL: without this, writing to a socket the
    // client already closed raises SIGPIPE and kills the whole server
    signal(SIGPIPE, SIG_IGN);

    pthread_create(&clean_ip_table_thread, NULL, cleanup_ip_table, NULL);
    pthread_detach(clean_ip_table_thread);

    pthread_t workers[WORKER_COUNT];

    for(int i=0;i<WORKER_COUNT;i++)
    {
        if (pthread_create(&workers[i], NULL, worker_loop, (void*)(intptr_t)server_fds[i]) != 0) {
            perror("pthread_create");
            return 1;
        }
    }
    for (int i = 0; i < WORKER_COUNT; i++) {
        pthread_join(workers[i], NULL);
    }

    for (int i = 0; i < WORKER_COUNT; i++) {
        close(server_fds[i]);
    }
    return 0;


}