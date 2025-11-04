CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -g -pthread
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

all: $(SERVER_BIN) $(CLIENT_BIN)

$(SERVER_BIN): $(COMMON_OBJS) $(SERVER_OBJS)
	@echo "Linking server..."
	$(CC) $(LDFLAGS) $^ -o $@

$(CLIENT_BIN): $(COMMON_OBJS) $(CLIENT_OBJS)
	@echo "Linking client..."
	$(CC) $(LDFLAGS) $^ -o $@ $(RAYLIB_LIBS)

obj/common/%.o: common/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

obj/server/%.o: server/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -Icommon -c $< -o $@

obj/client/%.o: client/%.c
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -Icommon -c $< -o $@

clean:
	@echo "Cleaning up..."
	rm -rf obj/*.o $(SERVER_BIN) $(CLIENT_BIN)

.PHONY: all clean
