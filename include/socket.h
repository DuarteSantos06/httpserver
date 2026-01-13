#pragma once

#include "client.h"
#include <sys/types.h>
#include <sys/event.h> 



// Cria e retorna o socket do servidor já bindado e a ouvir
int server_socket(int port);

// Aceita clientes no kqueue e regista os eventos de leitura
void accept_clients(int kq, int server_fd);

// Processa os eventos de leitura/escrita de um cliente
void handle_client_event(int kq, struct kevent *kev);

// Cria uma struct client inicializada para um fd
struct client* create_client(int client_fd);
