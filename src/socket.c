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


#include "server.h"
#include "client.h"
#include "response.h"
#include "http.h"
#include "treatiptable.h"
#include "request.h"
#include "handle_http_request.h"


struct client* create_client(int client_fd);


int read_from_client(struct client *c);
int write_to_client(struct client *c);
void close_client(int kq,struct client *c);
void set_to_write(struct client *c,int kq);

int server_socket(int port)
{
    int server_fd =socket(AF_INET6, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }
    int flags = fcntl(server_fd, F_GETFL, 0);
    if (flags == -1) { perror("fcntl get"); return -1; }
    if (fcntl(server_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl set"); close(server_fd); return -1;
    }
    int yes = 1;
    #ifdef SO_REUSEPORT
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) < 0)
        perror("setsockopt SO_REUSEPORT");
    #endif

    #ifdef IPV6_V6ONLY
    int off = 0;
    if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &off, sizeof(off)) < 0)
        perror("setsockopt IPV6_V6ONLY");
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
    if((listen(server_fd,SOMAXCONN)<0))
    {
        perror("Listen failled");
        return -1;
    }
    return server_fd;
}

int get_Ip(struct sockaddr_storage *cli_addr, int client_fd,char* client_ip ,size_t ip_len){
    if (cli_addr->ss_family == AF_INET) {
            struct sockaddr_in *addr4 = (struct sockaddr_in *)cli_addr;
            if (inet_ntop(AF_INET, &addr4->sin_addr, client_ip, ip_len) == NULL) {
                perror("inet_ntop v4");
                close(client_fd);
                return -1;
            }
        } else if (cli_addr->ss_family == AF_INET6) {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)cli_addr;
            if (inet_ntop(AF_INET6, &addr6->sin6_addr, client_ip, ip_len) == NULL) {
                perror("inet_ntop v6");
                close(client_fd);
                return -1;
            }
        }
        else {
            close(client_fd);
            return -1;
        }
        return 0;
}


void accept_clients(int kq, int server_fd)
{
    while(running)
    {
        struct sockaddr_storage cli_addr;
        socklen_t len=sizeof(cli_addr);
        
        int client_fd=accept(server_fd,(struct sockaddr*)&cli_addr,&len);

        if(client_fd<0){
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break; 
            perror("accept");
            break;
        }
        fcntl(client_fd,F_SETFL,O_NONBLOCK);
        char client_ip[INET6_ADDRSTRLEN]; 
        size_t ip_len = sizeof(client_ip);
        if(get_Ip(&cli_addr,client_fd,client_ip,ip_len)==-1){
            continue;
        }
        struct client *c=create_client(client_fd);
        if (strcmp(client_ip, "127.0.0.1") == 0 || strcmp(client_ip, "::1") == 0) {
         //skip rate limiting for localhost
        }
        else if(isRateLimited(client_ip)){
            prepare_429_response(c);
            set_to_write(c,kq);  
            continue;
        }
        struct kevent ev_client;
        EV_SET(&ev_client, client_fd, EVFILT_READ, EV_ADD, 0, 0, c);
        kevent(kq, &ev_client, 1, NULL, 0, NULL);

        addClientIpToTable(client_ip);
    }
}

void handle_header_parsing(struct client *c,int kq)
{
    char *end = strstr(c->buffer_in, "\r\n\r\n");
    if (!end)
        return;

        c->header_len = (end - c->buffer_in) + 4;

        struct request tmp;
    if (parse_request(c->buffer_in, &tmp) != 0) {
        prepare_response(c, 400, "Bad Request\n");
        set_to_write(c,kq);
        return;
    }

    c->body_expected = tmp.content_length;
}

void set_to_write(struct client *c,int kq)
{
    c->state = C_WRITING;
    struct kevent ev;
    EV_SET(&ev, c->fd, EVFILT_WRITE, EV_ADD, 0, 0, c);
    kevent(kq, &ev, 1, NULL, 0, NULL);
}

void handle_client_event(int kq,struct kevent *kev )
{
    struct client *c=(struct client *)kev->udata;

    if(kev->filter == EVFILT_READ && c->state == C_READING){
        int n=read_from_client(c);
        if(n==-1){
            g_connections_open--;
            c->state = C_CLOSED;
            close_client(kq,c);
            return;
        }
        if(n==-2){
            prepare_response(c,413,"Payload too large\n");
            set_to_write(c,kq);
            return;
        }
        if (c->header_len == 0) {
            handle_header_parsing(c,kq);
        }
        if (c->in_len < c->header_len + c->body_expected)
            return;
        handle_http_request(c);
        set_to_write(c,kq);
    }else if(kev->filter==EVFILT_WRITE &&c->state==C_WRITING)
    {
        if(write_to_client(c))
        {
            close_client(kq,c);
        }
    }
}

void close_client(int kq,struct client *c)
{
    close(c->fd);
    g_connections_open--;
    c->state=C_CLOSED;

    struct kevent ev;
    EV_SET(&ev, c->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
    kevent(kq, &ev, 1, NULL, 0, NULL);
    EV_SET(&ev,c->fd,EVFILT_WRITE,EV_DELETE,0,0,NULL);
    kevent(kq,&ev,1,NULL,0,NULL);
    free(c);
}

int write_to_client(struct client *c){
    int n = send(c->fd,c->buffer_out+c->out_sent,c->out_len-c->out_sent,0);
    if (n > 0)
        c->out_sent += n;
    else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
        return 1;    

    return c->out_sent >= c->out_len;
}

int read_from_client(struct client *c)
{
    int n = recv(c->fd,c->buffer_in+c->in_len,sizeof(c->buffer_in)-c->in_len-1,0);

    if (n == 0)
        return -1;

    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return 0;
        return -1;
    }

    c->in_len += n;
    c->buffer_in[c->in_len] = '\0';

    if (c->in_len >= MAX_HEADER_SIZE)
        return -2;

    return strstr(c->buffer_in, "\r\n\r\n") != NULL;
}

struct client* create_client(int client_fd)
{
    struct client*c=malloc(sizeof(struct client));
    if (!c) {
        perror("malloc client");
        return NULL;
    }
    memset(c,0,sizeof(struct client));
    c->fd=client_fd;
    c->state=C_READING;
    g_connections_open++;

    return c;
}