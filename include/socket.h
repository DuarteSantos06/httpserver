#pragma once

#include "client.h"
#include <sys/epoll.h>
#include <sys/types.h>


// Cria e retorna o socket do servidor já bindado e a ouvir
int server_socket(int port);

// Accept clients in a loop until there are no more to accept
void accept_clients(int epfd, int server_fd);

// Processa os eventos de leitura/escrita de um cliente
void handle_client_event(int epfd, struct epoll_event *event);

// Cria uma struct client inicializada para um fd
struct client* create_client(int client_fd,const char *client_ip);
