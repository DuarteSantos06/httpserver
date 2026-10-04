#include <string.h>
#include <time.h>
#include "test.h"
#include "client.h"
#include "list.h"
#include "socket.h"

// Stub for socket.c's close_client: clean() calls it, and the real one needs
// epoll and real sockets. Records the calls and unlinks like the real one.
static struct client *closed[8];
static int closed_count;
static int closed_epfd;

void close_client(int epfd, struct client *c)
{
    closed_epfd = epfd;
    if (closed_count < 8)
        closed[closed_count] = c;
    closed_count++;
    remove_client(c);
}

static void reset_list(void)
{
    clients_head = NULL;
    clients_tail = NULL;
    closed_count = 0;
    closed_epfd = -1;
}

static void test_add_to_empty_list(void)
{
    reset_list();
    struct client a;
    memset(&a, 0, sizeof(a));
    add_client(&a);
    EXPECT(clients_head == &a);
    EXPECT(clients_tail == &a);
    EXPECT(a.prev == NULL);
    EXPECT(a.next == NULL);
}

static void test_add_appends_to_tail(void)
{
    reset_list();
    struct client a, b, c;
    add_client(&a);
    add_client(&b);
    add_client(&c);
    EXPECT(clients_head == &a);
    EXPECT(clients_tail == &c);
    EXPECT(a.next == &b && b.next == &c && c.next == NULL);
    EXPECT(c.prev == &b && b.prev == &a && a.prev == NULL);
}

static void test_remove_head(void)
{
    reset_list();
    struct client a, b, c;
    add_client(&a);
    add_client(&b);
    add_client(&c);
    remove_client(&a);
    EXPECT(clients_head == &b);
    EXPECT(b.prev == NULL);
    EXPECT(clients_tail == &c);
}

static void test_remove_middle(void)
{
    reset_list();
    struct client a, b, c;
    add_client(&a);
    add_client(&b);
    add_client(&c);
    remove_client(&b);
    EXPECT(a.next == &c);
    EXPECT(c.prev == &a);
    EXPECT(clients_head == &a && clients_tail == &c);
}

static void test_remove_tail(void)
{
    reset_list();
    struct client a, b;
    add_client(&a);
    add_client(&b);
    remove_client(&b);
    EXPECT(clients_tail == &a);
    EXPECT(a.next == NULL);
}

static void test_remove_only_element(void)
{
    reset_list();
    struct client a;
    add_client(&a);
    remove_client(&a);
    EXPECT(clients_head == NULL);
    EXPECT(clients_tail == NULL);
}

static void test_move_to_tail(void)
{
    // Pattern used on activity: remove + add moves a client to the tail
    reset_list();
    struct client a, b, c;
    add_client(&a);
    add_client(&b);
    add_client(&c);
    remove_client(&a);
    add_client(&a);
    EXPECT(clients_head == &b);
    EXPECT(clients_tail == &a);
    EXPECT(c.next == &a && a.prev == &c);
}

static void test_clean_closes_only_idle_prefix(void)
{
    reset_list();
    time_t now = time(NULL);
    struct client a, b, c;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    memset(&c, 0, sizeof(c));
    a.last_activity = now - TIME_OUT - 5;
    b.last_activity = now - TIME_OUT;
    c.last_activity = now;
    add_client(&a);
    add_client(&b);
    add_client(&c);

    clean(3);
    EXPECT_EQ_INT(closed_count, 2);
    EXPECT(closed[0] == &a);
    EXPECT(closed[1] == &b);
    EXPECT_EQ_INT(closed_epfd, 3);
    EXPECT(clients_head == &c);
    EXPECT(clients_tail == &c);
}

static void test_clean_stops_at_first_active(void)
{
    // The list is ordered by activity, so clean() stops at the first active one
    reset_list();
    time_t now = time(NULL);
    struct client a, b;
    memset(&a, 0, sizeof(a));
    memset(&b, 0, sizeof(b));
    a.last_activity = now;
    b.last_activity = now - TIME_OUT - 100;
    add_client(&a);
    add_client(&b);

    clean(3);
    EXPECT_EQ_INT(closed_count, 0);
}

static void test_clean_empty_list(void)
{
    reset_list();
    clean(3);
    EXPECT_EQ_INT(closed_count, 0);
}

void run_list_tests(void)
{
    printf("-- list.c\n");
    RUN_TEST(test_add_to_empty_list);
    RUN_TEST(test_add_appends_to_tail);
    RUN_TEST(test_remove_head);
    RUN_TEST(test_remove_middle);
    RUN_TEST(test_remove_tail);
    RUN_TEST(test_remove_only_element);
    RUN_TEST(test_move_to_tail);
    RUN_TEST(test_clean_closes_only_idle_prefix);
    RUN_TEST(test_clean_stops_at_first_active);
    RUN_TEST(test_clean_empty_list);
    reset_list();
}
