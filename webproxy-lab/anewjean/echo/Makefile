CC = gcc
CFLAGS = -g -O0 -fno-omit-frame-pointer -Wall -Wextra
TARGET = echoserveri
OBJS = echoserveri.o csapp.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
