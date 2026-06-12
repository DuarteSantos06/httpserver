#include <time.h>
#include <stdio.h>
#include "client.h"
#include "audit.h"

void log_audit(struct client *c, const char *reason, const char *bad_path) {
    FILE *log_file = fopen("audit/audit.log", "a");
    if (!log_file) return; 

    time_t now = time(NULL);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(log_file, "[%s] [ALERT] IP: %s (FD: %d) | Motivo: %s | Path: %s\n", 
            time_str, c->ip, c->fd, reason, bad_path);

    fclose(log_file);
}