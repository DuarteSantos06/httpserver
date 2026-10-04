#pragma once

#include <stdio.h>
#include <string.h>

// Minimal test framework: each test is a void function, failures are counted
// and the runner returns non-zero if any assertion failed.

extern int g_tests_run;
extern int g_tests_failed;
extern int g_current_failed;

#define EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "  %s:%d: expected %s\n", __FILE__, __LINE__, #cond); \
        g_current_failed = 1; \
    } \
} while (0)

#define EXPECT_EQ_INT(a, b) do { \
    long long _a = (long long)(a), _b = (long long)(b); \
    if (_a != _b) { \
        fprintf(stderr, "  %s:%d: %s == %s (%lld != %lld)\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        g_current_failed = 1; \
    } \
} while (0)

#define EXPECT_EQ_STR(a, b) do { \
    const char *_a = (a), *_b = (b); \
    if (strcmp(_a, _b) != 0) { \
        fprintf(stderr, "  %s:%d: %s == %s (\"%s\" != \"%s\")\n", \
                __FILE__, __LINE__, #a, #b, _a, _b); \
        g_current_failed = 1; \
    } \
} while (0)

#define EXPECT_CONTAINS(haystack, needle) do { \
    const char *_h = (haystack), *_n = (needle); \
    if (!strstr(_h, _n)) { \
        fprintf(stderr, "  %s:%d: \"%s\" not found in %s\n", \
                __FILE__, __LINE__, _n, #haystack); \
        g_current_failed = 1; \
    } \
} while (0)

#define RUN_TEST(fn) do { \
    g_current_failed = 0; \
    g_tests_run++; \
    fn(); \
    if (g_current_failed) { \
        g_tests_failed++; \
        printf("[FAIL] %s\n", #fn); \
    } else { \
        printf("[ OK ] %s\n", #fn); \
    } \
} while (0)

// Test suites, one per source file under test
void run_http_tests(void);
void run_response_tests(void);
void run_list_tests(void);
void run_treatiptable_tests(void);
void run_router_tests(void);
