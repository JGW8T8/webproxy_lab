#include <stdio.h>
#include "csapp.h"
#include "cache.h"

// 전역 변수: 캐시 연결 리스트의 시작과 끝, 전체 크기
CachedObject *rootp;              // 가장 최근에 사용된 객체 (head of LRU)
CachedObject *lastp;              // 가장 오래된 객체 (tail of LRU)
int total_cache_size = 0;         // 현재 캐시에 저장된 전체 크기


// LRU 리스트는 rootp에서 시작해 lastp까지 이어지는 양방향 연결 리스트
// read_cache()가 접근 시 root로 당기고, write_cache()는 새로 추가
// 용량 초과 시 lastp부터 제거


// 클라이언트 요청 도착: find_cache()로 캐시 존재 여부 확인
// 캐시 히트: send_cache()로 클라이언트에 전달 + read_cache()로 LRU 갱신
// 캐시 미스: 원 서버에서 응답 수신 → 조건 충족 시 write_cache()로 저장
// 캐시 초과: write_cache() 내에서 자동으로 lastp 제거하며 용량 관리


// 요청한 path에 해당하는 객체가 캐시에 있는지 탐색
CachedObject *find_cache(char *path) 
{
  if (!rootp)                     // 캐시가 비어 있다면 NULL 반환
    return NULL;

  CachedObject *current = rootp; // LRU 맨 앞부터 탐색 시작

  while (strcmp(current->path, path))  // 현재 노드의 path가 일치하지 않으면
  {
    if (!current->next)          // 끝까지 갔는데 못 찾았으면 NULL 반환
      return NULL;

    current = current->next;     // 다음 노드로 이동

    if (!strcmp(current->path, path))  // 일치하는 path를 찾으면 해당 객체 반환
      return current;
  }

  return current;  // 루프를 빠져나왔다는 것은 처음 노드가 일치한 경우
}

// 클라이언트에게 캐시된 응답 데이터를 전송
void send_cache(CachedObject *Cache, int clientfd)
{
  char buf[MAXLINE];

  // 응답 헤더 구성
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n\r\n", buf, Cache->content_length);

  // 헤더 전송
  Rio_writen(clientfd, buf, strlen(buf));

  // 응답 바디 전송
  Rio_writen(clientfd, Cache->response_ptr, Cache->content_length);
}

// 접근한 캐시 객체를 리스트의 가장 앞 (root)로 이동 (LRU 정책 유지 목적)
void read_cache(CachedObject *Cache)
{
  if (Cache == rootp) // 이미 가장 앞에 있는 경우는 무시
    return;

  // 연결 해제: 현재 노드를 리스트에서 분리
  if (Cache->next)  // 중간 노드인 경우
  {
    CachedObject *prev_objtect = Cache->prev;
    CachedObject *next_objtect = Cache->next;

    if (prev_objtect)
      Cache->prev->next = next_objtect;
    Cache->next->prev = prev_objtect;
  }
  else  // 현재 노드가 tail (마지막 노드)인 경우
  {
    Cache->prev->next = NULL;  // 이전 노드가 새 tail이 됨
    lastp = Cache->prev;
  }

  // 현재 노드를 리스트의 맨 앞으로 이동
  Cache->next = rootp;
  rootp->prev = Cache;
  Cache->prev = NULL;
  rootp = Cache;
}

// 새로운 캐시 객체를 연결 리스트에 추가
void write_cache(CachedObject *Cache)
{
  // 총 캐시 크기 갱신
  total_cache_size += Cache->content_length;

  // 최대 캐시 크기 초과 시 가장 오래된 항목부터 제거
  while (total_cache_size > MAX_CACHE_SIZE)
  {
    total_cache_size -= lastp->content_length;  // 캐시 크기 감소

    lastp = lastp->prev;        // 이전 노드를 새 tail로 설정
    free(lastp->next);          // 이전 tail 메모리 해제
    lastp->next = NULL;         // 새 tail의 next는 NULL
  }

  // 처음 캐시 추가인 경우 (rootp가 NULL)
  if (!rootp)
    lastp = Cache;

  // 새 객체를 root로 삽입
  if (rootp)
  {
    Cache->next = rootp;
    rootp->prev = Cache;
  }

  Cache->prev = NULL;
  rootp = Cache;
}
