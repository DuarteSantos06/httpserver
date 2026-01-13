# -------------------------
# Configurações
# -------------------------
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pthread -g -Iinclude -I/opt/homebrew/include
TARGET = httpserver

SRC_DIR = src
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/loop.c $(SRC_DIR)/socket.c $(SRC_DIR)/http.c $(SRC_DIR)/response.c $(SRC_DIR)/treatiptable.c
OBJS = $(SRCS:.c=.o)
INCLUDE_DIR = include
DEPS = $(INCLUDE_DIR)/client.h $(INCLUDE_DIR)/loop.h $(INCLUDE_DIR)/socket.h $(INCLUDE_DIR)/http.h $(INCLUDE_DIR)/response.h $(INCLUDE_DIR)/server.h $(INCLUDE_DIR)/treatiptable.h

# -------------------------
# Regras
# -------------------------

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
