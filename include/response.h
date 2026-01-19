#pragma once

void prepare_response(struct client *c, int code, const char *body);
void prepare_status_response(struct client *c);