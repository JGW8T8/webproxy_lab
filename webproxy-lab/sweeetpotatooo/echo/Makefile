CC = gcc
CFLAGS = -Wall -I.

# 실행 파일 목록
all: echoserveri echoClient

# 서버: echoserveri.c + csapp.c
echoserveri: echoserveri.c csapp.c csapp.h
	$(CC) $(CFLAGS) -o echoserveri echoserveri.c csapp.c

# 클라이언트: echoclient.c + csapp.c
echoClient: echoclient.c csapp.c csapp.h
	$(CC) $(CFLAGS) -o echoClient echoclient.c csapp.c

clean:
	rm -f echoserveri echoClient *.o
