CC      = gcc
CFLAGS  = -Wall -Wextra
LDFLAGS = -lpthread

# Directories
BIN_DIR    = bin
LIB_DIR    = lib
SHARED_DIR = shared
SERVER_DIR = server
CLIENT_DIR = client

# cJSON
CJSON_SRC = $(LIB_DIR)/cJSON.c
CJSON_OBJ = $(LIB_DIR)/cJSON.o

# Shared library
SHARED_SRCS = $(shell find $(SHARED_DIR) -name '*.c' 2>/dev/null)
SHARED_OBJS = $(SHARED_SRCS:.c=.o)
SHARED_LIB  = $(LIB_DIR)/libshared.a

# Server
SERVER_SRCS = $(shell find $(SERVER_DIR) -name '*.c' 2>/dev/null)
SERVER_OBJS = $(SERVER_SRCS:.c=.o)
SERVER_BIN  = $(BIN_DIR)/server

# Client
CLIENT_SRCS = $(shell find $(CLIENT_DIR) -name '*.c' 2>/dev/null)
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)
CLIENT_BIN  = $(BIN_DIR)/client

# Include paths
INCLUDES = -I$(LIB_DIR) -I$(SHARED_DIR)

.PHONY: all clean shared server client dirs

all: dirs shared server client

dirs:
	@mkdir -p $(BIN_DIR) $(LIB_DIR)

# Compile cJSON
$(CJSON_OBJ): $(CJSON_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build shared static library
$(SHARED_LIB): $(SHARED_OBJS)
	ar rcs $@ $^

shared: dirs $(SHARED_LIB)

# Generic rule for .c -> .o
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build server binary
$(SERVER_BIN): $(SERVER_OBJS) $(SHARED_LIB) $(CJSON_OBJ)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) $(CJSON_OBJ) -L$(LIB_DIR) -lshared $(LDFLAGS)

server: dirs shared $(SERVER_BIN)

# Build client binary
$(CLIENT_BIN): $(CLIENT_OBJS) $(SHARED_LIB) $(CJSON_OBJ)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS) $(CJSON_OBJ) -L$(LIB_DIR) -lshared $(LDFLAGS)

client: dirs shared $(CLIENT_BIN)

clean:
	rm -f $(SHARED_OBJS) $(SERVER_OBJS) $(CLIENT_OBJS) $(CJSON_OBJ)
	rm -f $(SHARED_LIB)
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
