CC = gcc
CFLAGS = -Wall -Wextra -std=c2x -g -pthread
LDFLAGS = -pthread
RAYLIB_LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

COMMON_SRCS = $(wildcard common/*.c)
SERVER_SRCS = $(wildcard server/*.c)
CLIENT_SRCS = $(wildcard client/*.c)

COMMON_OBJS = $(patsubst common/%.c, obj/common/%.o, $(COMMON_SRCS))
SERVER_OBJS = $(patsubst server/%.c, obj/server/%.o, $(SERVER_SRCS))
CLIENT_OBJS = $(patsubst client/%.c, obj/client/%.o, $(CLIENT_SRCS))

SERVER_BIN = bin/server
CLIENT_BIN = bin/client

# Directories to create
DIRS = bin obj/common obj/server obj/client

all: $(DIRS) $(SERVER_BIN) $(CLIENT_BIN)

# Create directories if they don't exist
$(DIRS):
	@mkdir -p $@

$(SERVER_BIN): $(COMMON_OBJS) $(SERVER_OBJS)
	@echo "Linking server..."
	$(CC) $(LDFLAGS) $^ -o $@

$(CLIENT_BIN): $(COMMON_OBJS) $(CLIENT_OBJS)
	@echo "Linking client..."
	$(CC) $(LDFLAGS) $^ -o $@ $(RAYLIB_LIBS)

obj/common/%.o: common/%.c | obj/common
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

obj/server/%.o: server/%.c | obj/server
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -Icommon -c $< -o $@

obj/client/%.o: client/%.c | obj/client
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -Icommon -c $< -o $@

clean:
	@echo "Cleaning up..."
	rm -rf obj bin

.PHONY: all clean $(DIRS)
