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


struct client* create_client(int client_fd);

void handle_http_request(struct client *c);
int read_from_client(struct client *c);
int write_to_client(struct client *c);
void close_client(int kq,struct client *c);

int server_socket(int port)
{
    int server_fd =socket(AF_INET6, SOCK_STREAM, 0);
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);


    if (server_fd < 0) {
        perror("Socket creation failed");
        return -1;
    }
    int on = 1;
    #ifdef SO_REUSEPORT
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on));
    #endif
    #ifdef IPV6_V6ONLY
    int off = 0;
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
        char client_ip[INET6_ADDRSTRLEN]; 
        if (cli_addr.ss_family == AF_INET) {
            struct sockaddr_in *addr4 = (struct sockaddr_in *)&cli_addr;
            if (inet_ntop(AF_INET, &addr4->sin_addr, client_ip, sizeof(client_ip)) == NULL) {
                perror("inet_ntop v4");
                close(client_fd);
                continue;
            }
        } else if (cli_addr.ss_family == AF_INET6) {
            struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)&cli_addr;
            if (inet_ntop(AF_INET6, &addr6->sin6_addr, client_ip, sizeof(client_ip)) == NULL) {
                perror("inet_ntop v6");
                close(client_fd);
                continue;
            }
        } else {
            // Família que não queremos tratar
            close(client_fd);
            continue;
        }
        if (strcmp(client_ip, "127.0.0.1") == 0 || strcmp(client_ip, "::1") == 0) {
         // skip rate limit
        }
        else if(isRateLimited(client_ip)){
            close(client_fd);
            continue;
        }
        
        fcntl(client_fd,F_SETFL,O_NONBLOCK);

        struct client *c=create_client(client_fd);
        g_connections_open++;

        struct kevent ev_client;
        EV_SET(&ev_client, client_fd, EVFILT_READ, EV_ADD, 0, 0, c);
        kevent(kq, &ev_client, 1, NULL, 0, NULL);

        
        addClientIpToTable(client_ip);
    }
}

void handle_client_event(int kq,struct kevent *kev )
{
    struct client *c=(struct client *)kev->udata;

    if(kev->filter == EVFILT_READ && c->state == C_READING){

        int n=read_from_client(c);
        if(n==-1){
            close(c->fd);
            g_connections_open--;
            c->state = C_CLOSED;
            return;
        }
        if(n==-2){
            prepare_response(c,413,"Payload too large\n");
            c->state=C_WRITING;

            struct kevent ev;
            EV_SET(&ev,c->fd,EVFILT_WRITE,EV_ADD,0,0,c);
            kevent(kq,&ev,1,NULL,0,NULL);
            return;
        }
        c->buffer_in[c->in_len]='\0';

        char *header_end = strstr(c->buffer_in, "\r\n\r\n");
        if (header_end == NULL) {
            return;
        }
        handle_http_request(c);
        c->state = C_WRITING;
        struct kevent ev;
        EV_SET(&ev, c->fd, EVFILT_WRITE, EV_ADD, 0, 0, c);
        kevent(kq, &ev, 1, NULL, 0, NULL);
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
    EV_SET(&ev,c->fd,EVFILT_WRITE,EV_DELETE,0,0,NULL);
    kevent(kq,&ev,1,NULL,0,NULL);
    free(c);
}

int write_to_client(struct client *c){
    int n = send(c->fd,c->buffer_out+c->out_sent,c->out_len-c->out_sent,0);
    if (n > 0)
        c->out_sent += n;

    return c->out_sent >= c->out_len;
}

int read_from_client(struct client *c)
{
    int n = recv(c->fd,c->buffer_in+c->in_len,sizeof(c->buffer_in)-c->in_len,0);

    if (n <= 0) return -1;

    c->in_len += n;
    c->buffer_in[c->in_len] = '\0';

    if (c->in_len >= MAX_HEADER_SIZE)
        return -2;

    return strstr(c->buffer_in, "\r\n\r\n") != NULL;
}

void handle_http_request(struct client *c)
{
    struct request req;
        if(parse_request(c->buffer_in,&req)!=0){
            prepare_response(c,400,"Bad request\n");
        }else{
            if(strcmp(req.path,"/")==0){
                prepare_response(c,200,"Sucess");
            }
            else if (strcmp(req.path,"/status")==0)
            {
                prepare_status_response(c);
            }
            else {
                prepare_response(c,404,"Not found");
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