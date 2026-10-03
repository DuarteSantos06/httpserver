#pragma once

#include "client.h"

#define TIME_OUT 15

_Thread_local extern struct client *clients_head;
_Thread_local extern struct client *clients_tail;

// Appends the client to the tail of the list (most recently active)
void add_client(struct client *c);

// Unlinks the client from the list (does not close or free it)
void remove_client(struct client *c);

// Closes clients that have been idle for more than TIME_OUT seconds
void clean(int epfd);