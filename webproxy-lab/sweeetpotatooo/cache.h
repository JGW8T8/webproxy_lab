//webproxy-lab/sweeetpotatooo/cache.h

#include <stdio.h>

#include "csapp.h"

typedef struct CachedObject
{
  char path[MAXLINE];                 // 요청 경로(URI) -> 캐시 키  
  int content_length;                 // 응답 바디 길이
  char *response_ptr;                 // 응답 바디 데이터
  struct CachedObject *prev, *next;   // Doubly Linked List(LRU)
} CachedObject;

CachedObject *find_cache(char *path);
void send_cache(CachedObject *Cache, int clientfd);
void read_cache(CachedObject *Cache);
void write_cache(CachedObject *Cache);

extern CachedObject *rootp;  // 캐시 연결리스트의 root 객체
extern CachedObject *lastp;  // 캐시 연결리스트의 마지막 객체
extern int total_cache_size; // 캐싱된 객체 크기의 총합

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400