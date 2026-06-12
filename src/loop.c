#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <stdint.h>    
#include <pthread.h>

#include "loop.h"
#include "socket.h"
#include "client.h"
#include "http.h"
#include "response.h"
#include "server.h"


void* worker_loop(void *arg)
{
    int server_fd=(int)(intptr_t)arg;
    int epfd = epoll_create1(0);  // Create the epoll instance
    if (epfd < 0) {
        perror("epoll_create1");
        return NULL;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;          
    ev.data.fd = server_fd;

    // Add the server fd to the epoll instance
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &ev) == -1) {
        perror("epoll_ctl: server_fd");
        close(epfd);
        return NULL;
    }

    struct epoll_event events[1024];
    int timeout_ms = 1000;

    while(running)
    {
        int n = epoll_wait(epfd, events, 1024, timeout_ms);
        if (n < 0) {
            if (errno == EINTR) continue; 
            perror("epoll_wait");
            break;
        }

        for(int i=0;i<n;i++)
        {
            int fd = events[i].data.fd;
    
            if(fd==server_fd)
            {
                accept_clients(epfd, server_fd);
            }else{
                handle_client_event(epfd, &events[i]);
            }
        }
    }
    close(epfd);
    return NULL;
}