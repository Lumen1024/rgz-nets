CC      = gcc
CFLAGS  = -Wall -Wextra -std=gnu11 -g
LDFLAGS = -pthread

all: server client

server: server.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

client: client.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f server client chat_*.txt

.PHONY: all clean
