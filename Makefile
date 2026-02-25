CC     = gcc
CFLAGS = -Wall -Wextra -O2 -pthread

COMMON  = common/net_utils.c
SRV_SRC = server/main.c server/shared.c server/history.c server/handler.c $(COMMON)
CLI_SRC = client/main.c client/scanner.c client/session.c $(COMMON)

all: server client

server: $(SRV_SRC)
	$(CC) $(CFLAGS) -o chat_server $^

client: $(CLI_SRC)
	$(CC) $(CFLAGS) -o chat_client $^

clean:
	rm -f chat_server chat_client
