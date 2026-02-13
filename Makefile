CC := g++
CFLAGS := -std=c++17 -pthread -Iserver
SRCS := server/main.cpp server/reactor/epoll_reactor.cpp \
        server/thread/thread_pool.cpp server/client/client_manager.cpp \
        server/message/message_parser.cpp server/message/message_forwarder.cpp \
        server/heartbeat/heartbeat_checker.cpp server/utils/config.cpp
TARGET := chat_server

.PHONY: all build run clean

all: build

build:
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

run: build
	./$(TARGET)

clean:
	rm -f $(TARGET)
