CC      = gcc
CFLAGS  = -Wall -Wextra
LDFLAGS = -lpthread

# Directories
BIN_DIR    = bin
OBJ_DIR    = bin/obj
LIB_DIR    = lib
SHARED_DIR = shared
SERVER_DIR = server
CLIENT_DIR = client

# cJSON
CJSON_SRC = $(LIB_DIR)/cJSON.c
CJSON_OBJ = $(OBJ_DIR)/cJSON.o

# Shared library
SHARED_SRCS = $(shell find $(SHARED_DIR) -name '*.c' 2>/dev/null)
SHARED_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SHARED_SRCS))
SHARED_LIB  = $(LIB_DIR)/libshared.a

# Server
SERVER_SRCS = $(shell find $(SERVER_DIR) -name '*.c' 2>/dev/null)
SERVER_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(SERVER_SRCS))
SERVER_BIN  = $(BIN_DIR)/server

# Client
CLIENT_SRCS = $(shell find $(CLIENT_DIR) -name '*.c' 2>/dev/null)
CLIENT_OBJS = $(patsubst %.c,$(OBJ_DIR)/%.o,$(CLIENT_SRCS))
CLIENT_BIN  = $(BIN_DIR)/client

# Include paths
INCLUDES = -I$(LIB_DIR) -I$(SHARED_DIR) -I$(SHARED_DIR)/protocol -I$(SERVER_DIR) -I$(SERVER_DIR)/handlers -I$(SERVER_DIR)/repositories -I$(SERVER_DIR)/notify -I$(CLIENT_DIR)

.PHONY: all clean shared server client dirs

all: dirs shared server client

dirs:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR) $(LIB_DIR)
	@mkdir -p $(OBJ_DIR)/$(SHARED_DIR)
	@mkdir -p $(OBJ_DIR)/$(SHARED_DIR)/protocol
	@mkdir -p $(OBJ_DIR)/$(SERVER_DIR)/handlers
	@mkdir -p $(OBJ_DIR)/$(SERVER_DIR)/repositories
	@mkdir -p $(OBJ_DIR)/$(SERVER_DIR)/notify
	@mkdir -p $(OBJ_DIR)/$(CLIENT_DIR)
	@mkdir -p $(OBJ_DIR)/$(LIB_DIR)

# Compile cJSON
$(CJSON_OBJ): $(CJSON_SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Generic rule for .c -> bin/obj/...
$(OBJ_DIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Build shared static library
$(SHARED_LIB): $(SHARED_OBJS)
	ar rcs $@ $^

shared: dirs $(SHARED_LIB)

# Build server binary
$(SERVER_BIN): $(SERVER_OBJS) $(SHARED_LIB) $(CJSON_OBJ)
	$(CC) $(CFLAGS) -o $@ $(SERVER_OBJS) $(CJSON_OBJ) -L$(LIB_DIR) -lshared $(LDFLAGS)

server: dirs shared $(SERVER_BIN)

# Build client binary
$(CLIENT_BIN): $(CLIENT_OBJS) $(SHARED_LIB) $(CJSON_OBJ)
	$(CC) $(CFLAGS) -o $@ $(CLIENT_OBJS) $(CJSON_OBJ) -L$(LIB_DIR) -lshared $(LDFLAGS) -lncurses

client: dirs shared $(CLIENT_BIN)

clean:
	rm -rf $(OBJ_DIR)
	rm -f $(SHARED_LIB)
	rm -f $(SERVER_BIN) $(CLIENT_BIN)
