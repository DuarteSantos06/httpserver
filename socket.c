#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/event.h>
#include <sys/time.h>

#include "socket.h"
#include "client.h"
#include "server.h"
#include "response.h"
#include "http.h"
#include "treatiptable.h"


struct client* create_client(int client_fd);

int server_socket(int port)
{
    int server_fd =socket(AF_INET6, SOCK_STREAM, 0);
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);


    if (server_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }
    int off = 0;
    #ifdef SO_REUSEPORT
    setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off));
    #endif
    struct sockaddr_in6 address6;
    memset(&address6, 0, sizeof(address6));
    address6.sin6_family = AF_INET6;
    address6.sin6_port = htons(port);
    address6.sin6_addr = in6addr_any;

    if((bind(server_fd,(struct sockaddr*)&address6,sizeof(address6)))<0){
        perror("could not bind");
        return -1;
    }
    if((listen(server_fd,1024)<0))
    {
        perror("Listen failled");
        return -1;
    }
    return server_fd;
}


void accept_clients(int kq, int server_fd)
{
    while(1)
    {
        struct sockaddr_in cli_addr;
        socklen_t len=sizeof(cli_addr);
        
        int client_fd=accept(server_fd,(struct sockaddr*)&cli_addr,&len);

        if(client_fd<0){
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; 
            perror("accept");
            break;
        }
        if(isRateLimited(cli_addr.sin_addr)){
            
            printf("IP %s is rate limited!\n", inet_ntoa(cli_addr.sin_addr));
            close(client_fd);
            continue;
        }
        
        fcntl(client_fd,F_SETFL,O_NONBLOCK);

        struct client *c=create_client(client_fd);

        struct kevent ev_client;
        EV_SET(&ev_client, client_fd, EVFILT_READ, EV_ADD, 0, 0, c);
        kevent(kq, &ev_client, 1, NULL, 0, NULL);

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET6, &cli_addr.sin_addr, client_ip, sizeof(client_ip));
        addClientIpToTable(client_ip);
    }
}

void handle_client_event(int kq,struct kevent *kev )
{
    struct client *c=(struct client *)kev->udata;

    if(kev->filter == EVFILT_READ && c->state == C_READING){
        int n=recv(c->fd,c->buffer_in+c->in_len,sizeof(c->buffer_in)-c->in_len,0);
        if(n<=0){
            close(c->fd);
            c->state = C_CLOSED;
            return;
        }
        c->in_len+=n;
        printf("Received %d bytes from client %d, buffer: %s\n", n, c->fd, c->buffer_in);

        char path[1024];

        if(parse_request(c->buffer_in,path)!=0){
            prepare_response(c,400,"Bad request\n");
        }else{
            if(strcmp(path,"/")==0){
                prepare_response(c,200,"Sucess");
            }else{
                prepare_response(c,404,"Not found");
            }
        }
        c->state = C_WRITING;
        struct kevent ev;
        EV_SET(&ev, c->fd, EVFILT_WRITE, EV_ADD, 0, 0, c);
        kevent(kq, &ev, 1, NULL, 0, NULL);
    }else if(kev->filter==EVFILT_WRITE &&c->state==C_WRITING)
    {
        int n=send(c->fd,c->buffer_out+c->out_sent,c->out_len-c->out_sent,0);
        if(n>0)
        {
            c->out_sent+=n;
        }
        if(c->out_sent>=c->out_len)
        {
            close(c->fd);
            c->state=C_CLOSED;

            struct kevent ev;
            EV_SET(&ev,c->fd,EVFILT_WRITE,EV_DELETE,0,0,NULL);
            kevent(kq,&ev,1,NULL,0,NULL);
            free(c);
        }
    }
}

struct client* create_client(int client_fd)
{
    struct client*c=malloc(sizeof(struct client));
    c->fd=client_fd;
    c->state=C_READING;
    c->in_len = 0;
    c->out_len = 0;
    c->out_sent = 0;

    return c;
}