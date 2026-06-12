#pragma once

void prepare_response(struct client *c, int code, const char *body);
void prepare_status_response(struct client *c);
void prepare_429_response(struct client *c);
int prepare_file_response(struct client *c, const char *file_path);