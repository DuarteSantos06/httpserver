#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "test.h"
#include "server.h"

int g_tests_run = 0;
int g_tests_failed = 0;
int g_current_failed = 0;

// Globals normally defined in src/main.c
atomic_bool running = 1;
atomic_ulong g_requests_total = 0;
atomic_ulong g_connections_open = 0;

int main(void)
{
    // Handlers use relative paths (www/, data/, audit/), so run inside a
    // scratch directory to avoid touching the real project files.
    char dir[] = "/tmp/httpserver_tests_XXXXXX";
    if (!mkdtemp(dir) || chdir(dir) != 0) {
        perror("test setup");
        return 1;
    }
    mkdir("www", 0755);
    mkdir("data", 0755);
    mkdir("audit", 0755);
    printf("Running tests in %s\n\n", dir);

    run_http_tests();
    run_response_tests();
    run_list_tests();
    run_treatiptable_tests();
    run_router_tests();

    printf("\n%d tests, %d failed\n", g_tests_run, g_tests_failed);
    return g_tests_failed ? 1 : 0;
}
