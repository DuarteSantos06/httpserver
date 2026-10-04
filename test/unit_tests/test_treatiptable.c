#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include "test.h"
#include "treatiptable.h"

// Same hash as treatiptable.c (static there), used to find entries directly
static unsigned long shard_of(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % MAX_SHARDS;
}

static ip_entry *find_entry(const char *ip)
{
    ip_entry *entry;
    HASH_FIND_STR(ip_map[shard_of(ip)], ip, entry);
    return entry;
}

// Waits for the start of a new second so time(NULL) won't tick mid-test
static void sync_to_second(void)
{
    time_t start = time(NULL);
    while (time(NULL) == start)
        ;
}

static void test_add_ip_creates_entry(void)
{
    char ip[] = "10.0.0.1";
    add_client_ip_to_table(ip);
    ip_entry *e = find_entry(ip);
    EXPECT(e != NULL);
    if (!e) return;
    EXPECT_EQ_STR(e->ip, "10.0.0.1");
    EXPECT(e->tokens == MAX_TOKENS);
}

static void test_add_ip_twice_keeps_one_entry(void)
{
    char ip[] = "10.0.0.2";
    add_client_ip_to_table(ip);
    ip_entry *first = find_entry(ip);
    if (first) first->tokens = 2;
    add_client_ip_to_table(ip);
    EXPECT(find_entry(ip) == first);
    // Re-adding must not reset the tokens
    if (first) EXPECT(first->tokens == 2);
}

static void test_unknown_ip_not_limited(void)
{
    EXPECT_EQ_INT(is_rate_limited("192.168.255.254"), 0);
}

static void test_burst_is_limited(void)
{
    char ip[] = "10.0.0.3";
    sync_to_second();
    add_client_ip_to_table(ip);
    for (int i = 0; i < MAX_TOKENS; i++)
        EXPECT_EQ_INT(is_rate_limited(ip), 0);
    EXPECT_EQ_INT(is_rate_limited(ip), 1);
    EXPECT_EQ_INT(is_rate_limited(ip), 1);
}

static void test_tokens_refill_over_time(void)
{
    char ip[] = "10.0.0.4";
    add_client_ip_to_table(ip);
    ip_entry *e = find_entry(ip);
    EXPECT(e != NULL);
    if (!e) return;
    e->tokens = 0;
    e->last_time = time(NULL) - 1; // one second ago -> RATE new tokens
    EXPECT_EQ_INT(is_rate_limited(ip), 0);
    EXPECT(e->tokens <= MAX_TOKENS - 1);
}

static void test_tokens_capped_at_max(void)
{
    char ip[] = "10.0.0.5";
    add_client_ip_to_table(ip);
    ip_entry *e = find_entry(ip);
    EXPECT(e != NULL);
    if (!e) return;
    e->last_time = time(NULL) - 1000;
    EXPECT_EQ_INT(is_rate_limited(ip), 0);
    EXPECT(e->tokens == MAX_TOKENS - 1);
}

static void test_ipv6_address(void)
{
    char ip[] = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
    add_client_ip_to_table(ip);
    ip_entry *e = find_entry(ip);
    EXPECT(e != NULL);
    if (e) EXPECT_EQ_STR(e->ip, ip);
}

static void free_table(void)
{
    ip_entry *cur, *tmp;
    for (int shard = 0; shard < MAX_SHARDS; shard++) {
        HASH_ITER(hh, ip_map[shard], cur, tmp) {
            HASH_DEL(ip_map[shard], cur);
            free(cur);
        }
    }
}

void run_treatiptable_tests(void)
{
    printf("-- treatiptable.c\n");
    RUN_TEST(test_add_ip_creates_entry);
    RUN_TEST(test_add_ip_twice_keeps_one_entry);
    RUN_TEST(test_unknown_ip_not_limited);
    RUN_TEST(test_burst_is_limited);
    RUN_TEST(test_tokens_refill_over_time);
    RUN_TEST(test_tokens_capped_at_max);
    RUN_TEST(test_ipv6_address);
    free_table();
}
