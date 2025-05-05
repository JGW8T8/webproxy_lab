CC = gcc
CFLAGS = -g -O0 -fno-omit-frame-pointer -Wall -Wextra
TARGET = echo_server
OBJS = echo_server.o csapp.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)