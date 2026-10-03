#include <stdio.h>
#include "client.h"
#include "list.h"
#include <time.h>
#include "socket.h"


_Thread_local struct client *clients_head = NULL;
_Thread_local struct client *clients_tail = NULL;

void add_client(struct client *c) {
    c->next = NULL;
    c->prev = clients_tail;

    if (clients_tail)
        clients_tail->next = c;  
    else
        clients_head = c;         

    clients_tail = c;
}

void remove_client(struct client *c) {
    if (c->prev)
        c->prev->next = c->next; 
    else
        clients_head = c->next;  

    if (c->next)
        c->next->prev = c->prev; 
    else
        clients_tail = c->prev;   // não havia cliente seguinte: o anterior é o último
}

void clean(int epfd)
{
    time_t now = time(NULL);
    struct client *c = clients_head;
    while (c) {
        if (now - c->last_activity < TIME_OUT) {
            break;
        }
        struct client *next = c->next;
        close_client(epfd, c);
        c = next;
    }
}