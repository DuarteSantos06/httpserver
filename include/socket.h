#pragma once

#include "client.h"
#include <sys/epoll.h>
#include <sys/types.h>


// Creates the server socket, already bound and listening, and returns it
int server_socket(int port);

// Accept clients in a loop until there are no more to accept
void accept_clients(int epfd, int server_fd);

// Handles read/write events for a client
void handle_client_event(int epfd, struct epoll_event *event);

// Allocates and initializes a struct client for an fd
struct client* create_client(int client_fd,const char *client_ip);

void close_client(int epfd,struct client *c);    
