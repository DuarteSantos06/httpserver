#include "request.h"
#include "client.h"

void handle_root_get(struct client *c,const struct request *req);
void handle_status_get(struct client *c,const struct request *req);
void handle_error_not_found(struct client *c);
void handle_data_post(struct client *c,const struct request *req);