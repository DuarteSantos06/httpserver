# -------------------------
# Unit tests (Linux only, like the server: run with `make -C test/unit_tests`)
# -------------------------
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pthread -g -I../../include -I. -fsanitize=address,undefined -fno-omit-frame-pointer
TARGET = run_tests

SRC_DIR = ../../src
# Everything except main.c, loop.c and socket.c (need real sockets/epoll);
# close_client is stubbed in test_list.c
SRCS = $(SRC_DIR)/http.c $(SRC_DIR)/response.c $(SRC_DIR)/list.c $(SRC_DIR)/treatiptable.c \
       $(SRC_DIR)/handle_http_request.c $(SRC_DIR)/handlers_utils.c $(SRC_DIR)/audit.c
TESTS = main.c test_http.c test_response.c test_list.c test_treatiptable.c test_router.c

all: test

$(TARGET): $(SRCS) $(TESTS) test.h $(wildcard ../../include/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(TESTS)

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all test clean
