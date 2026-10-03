#pragma once

#include "client.h"

#define TIME_OUT 15

_Thread_local extern struct client *clients_head;
_Thread_local extern struct client *clients_tail;

// Adiciona o cliente ao fim da lista (o mais recente)
void add_client(struct client *c);

// Remove o cliente da lista (não fecha nem liberta)
void remove_client(struct client *c);

// Fecha os clientes inativos há mais de TIME_OUT segundos
void clean(int epfd);