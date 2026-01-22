#include "handle_http_request.h"
#include "request.h"
#include "response.h"
#include "http.h"
#include <string.h>
#include "client.h"
#include <fcntl.h>
#include <unistd.h>
#include "handlers_utils.h"


typedef void (*route_handler_fn)(struct client *c, const struct request *req);

struct route_entry {
    const char      *method;
    const char      *path;
    route_handler_fn handler;
};

static const struct route_entry ROUTES[] = {
    { "GET",  "/",       handle_root_get   },
    { "GET",  "/status", handle_status_get },
    { "POST", "/data", handle_data_post }, 
};

void handle_http_request(struct client *c)
{
    struct request req;

    if (parse_request(c->buffer_in, &req) != 0) {
        prepare_response(c, 400, "Bad Request\n");
        return;
    }
    
    for(size_t i=0;i<sizeof(ROUTES) / sizeof(ROUTES[0]);i++){
        if(strcmp(req.method,ROUTES[i].method)==0 && strcmp(req.path,ROUTES[i].path)==0)
        {
            ROUTES[i].handler(c, &req);
            return;
        }
    }

    handle_error_not_found(c);
}