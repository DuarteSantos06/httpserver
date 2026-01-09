# -------------------------
# Configurações
# -------------------------
CC = gcc
CFLAGS = -Wall -Wextra -Werror -pthread -g -I/opt/homebrew/include
TARGET = httpserver

SRCS = main.c loop.c socket.c http.c response.c treatiptable.c
OBJS = $(SRCS:.c=.o)
DEPS = client.h loop.h socket.h http.h response.h server.h treatiptable.h

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
