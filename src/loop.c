#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/event.h>
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
    int kq=kqueue();

    struct kevent ev;
    EV_SET(&ev,server_fd,EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq,&ev,1,NULL,0,NULL);

    
    struct kevent events[1024];

    while(running)
    {
        
        int n=kevent(kq,NULL,0,events,1024,NULL);
        if(n<0){
            printf("error, n<0");
        }

        for(int i=0;i<n;i++)
        {
            int fd=(int)events[i].ident;
    
            if(fd==server_fd)
            {
                accept_clients(kq,server_fd);
            }else{
                handle_client_event(kq,&events[i]);
            }
        }
    }
    close(kq);
    return NULL;
}