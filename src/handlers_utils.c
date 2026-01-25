#include "handlers_utils.h"
#include "response.h"
#include "request.h"
#include "client.h"
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

void handle_root_get(struct client *c,const struct request *req)
{
    (void)req;
    prepare_response(c,200,"Success");
}

void handle_status_get(struct client *c,const struct request *req)
{
    (void)req;
    prepare_status_response(c);
}

void handle_error_not_found(struct client *c)
{
    prepare_response(c,404,"Not found");
}

void handle_data_post(struct client *c,const struct request *req)
{
    if(req->content_length <=0){
        prepare_response(c,400,"Bad request: No body\n");
        return;
    }
    int fd=open("data/data.txt",O_WRONLY|O_CREAT|O_APPEND,0644);
    if(fd==-1){
        prepare_response(c,500,"Internal Server Error: Cannot open file\n");
        return;
    }
    size_t total=0;
    while(total < (size_t)req->content_length){
        ssize_t written=write(fd,req->body + total, req->content_length - total);
        if(written==-1){
            prepare_response(c,500,"Internal Server Error: Cannot write to file\n");
            close(fd);
            return;
        }
        total += written;
    }
    close(fd);
    prepare_response(c,200,"Data received and stored\n");
}