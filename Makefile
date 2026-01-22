# -------------------------
# Configurações
# -------------------------
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pthread -g -Iinclude -I/opt/homebrew/include
TARGET = httpserver
BUILD_DIR = build

SRC_DIR = src
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/loop.c $(SRC_DIR)/socket.c $(SRC_DIR)/http.c $(SRC_DIR)/response.c $(SRC_DIR)/treatiptable.c $(SRC_DIR)/handle_http_request.c  $(SRC_DIR)/handlers_utils.c
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
INCLUDE_DIR = include
DEPS =  $(INCLUDE_DIR)/client.h $(INCLUDE_DIR)/loop.h $(INCLUDE_DIR)/socket.h $(INCLUDE_DIR)/http.h $(INCLUDE_DIR)/response.h $(INCLUDE_DIR)/server.h $(INCLUDE_DIR)/treatiptable.h $(INCLUDE_DIR)/request.h $(INCLUDE_DIR)/handle_http_request.h $(INCLUDE_DIR)/handlers_utils.h

# -------------------------
# Regras
# -------------------------

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(DEPS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
