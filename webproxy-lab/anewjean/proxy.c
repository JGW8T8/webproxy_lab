#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

typedef struct cache_block {
    char uri[MAXLINE];
    char *data;
    int size;
    struct cache_block *prev, *next;
} cache_block;

cache_block *head = NULL, *tail = NULL;
int cache_size = 0;
pthread_rwlock_t cache_lock;

static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void *thread(void *vargp);
void forward_request(int connfd);
int parse_uri(char *uri, char *host, char *port, char *path);
void cache_init();
cache_block *cache_find(const char *uri);
void cache_insert(const char *uri, const char *data, int size);
void cache_evict(int needed_size);

int main(int argc, char **argv)
{
    int listenfd, *connfdp; // listenfd는 지역변수로 관리하고, n개 생성될 connfd만 포인터로 관리 
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid; // 스레드 식별값

    if (argc != 2) { // port 번호를 입력하지 않았을 때 안내 메세지
        fprintf(stderr, "usage: %s <port>\n", argv[0]); 
        exit(1);
    }

    cache_init(); // 캐시 초기화
    listenfd = Open_listenfd(argv[1]); // 포트 번호를 인자로 받아서 소켓을 열고, listenfd에 저장
    while (1) {
        clientlen = sizeof(clientaddr); // 
        connfdp = malloc(sizeof(int)); // 새로운 connfd를 생성하기 위한 int 사이즈 메모리 블록 할당
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트의 연결 요청 수락 & 클라이언트와 통신할 수 있는 새로운 소켓 반환
        pthread_create(&tid, NULL, thread, connfdp); // 연결된 클라이언트의 요청을 처리하기 위한 스레드 생성
    }
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp); // connfd는 포인터로 전달되므로 역참조하여 실제 fd를 가져와 스택에 저장
    pthread_detach(pthread_self()); // 현재 스레드를 분리 상태로 만들어 스레드 종료 시 자동으로 리소스를 회수하게 함
    free(vargp); // 힙에 동적 할당했던 connfd 저장용 메모리 해제

    forward_request(connfd); // 클라이언트의 요청 처리 (프록시 기능 수행)
    Close(connfd); // 클라이언트와의 연결 종료
    return NULL;
}

void forward_request(int connfd)
{
    rio_t client_rio, server_rio;
    char buf[MAXLINE], req[MAX_OBJECT_SIZE];
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], port[10], path[MAXLINE];

    Rio_readinitb(&client_rio, connfd); // connfd에 대한 rio_t 구조체 초기화
    if (!Rio_readlineb(&client_rio, buf, MAXLINE)) return; // 클라이언트로부터 요청을 읽어옴
    sscanf(buf, "%s %s %s", method, uri, version); // 요청 메세지에서 메소드, URI, 버전 정보 파싱

    if (strcasecmp(method, "GET") != 0) return; // GET 메소드가 아닐 경우 무시
    if (parse_uri(uri, host, port, path) < 0) return; // URI 파싱 -> 호스트, 포트, 경로 정보 추출

    printf("[LOOKUP] %s\n", path);
    pthread_rwlock_rdlock(&cache_lock); // 캐시 락을 읽기 모드로 잠금
    cache_block *cb = cache_find(path); // 캐시에서 URI에 해당하는 블록 찾기
    if (cb) {
        printf("[HIT] %s\n", path);
        Rio_writen(connfd, cb->data, cb->size); // 캐시에서 찾은 경우 클라이언트에게 데이터 전송
        pthread_rwlock_unlock(&cache_lock); // 캐시에서 찾은 경우 읽기 락 해제
        return;
    }
    pthread_rwlock_unlock(&cache_lock); // 캐시에서 찾지 못한 경우 읽기 락 해제

    sprintf(req, "GET %s HTTP/1.0\r\n", path);
    while (Rio_readlineb(&client_rio, buf, MAXLINE) > 0 && strcmp(buf, "\r\n") != 0) { // 헤더 정보 읽기
        if (strncasecmp(buf, "Host:", 5) == 0 ||            // 호스트 정보
            strncasecmp(buf, "User-Agent:", 11) == 0 ||     // 사용자 에이전트 정보
            strncasecmp(buf, "Connection:", 11) == 0 ||     // 연결 정보
            strncasecmp(buf, "Proxy-Connection:", 17) == 0) // 프록시 연결 정보
            continue;
        strcat(req, buf);
    }
    sprintf(buf, "Host: %s\r\n", host); strcat(req, buf); // 호스트 정보 추가
    strcat(req, user_agent_hdr); // 사용자 에이전트 정보 추가
    strcat(req, "Connection: close\r\n"); // 연결 정보 추가
    strcat(req, "Proxy-Connection: close\r\n\r\n"); // 프록시 연결 정보 추가

    int serverfd = Open_clientfd(host, port); // end 서버와 연결하기 위한 소켓 생성
    if (serverfd < 0) return; // 서버와 연결 실패 시 종료

    Rio_readinitb(&server_rio, serverfd); // 서버와 연결된 소켓에 대한 rio_t 구조체 초기화
    Rio_writen(serverfd, req, strlen(req)); // 서버에 요청 전송

    char object_buf[MAX_OBJECT_SIZE]; // 캐시 저장을 위한 버퍼
    int total_size = 0, n; // 총 크기 및 읽은 바이트 수
    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) { // 서버로부터 응답 읽기
        Rio_writen(connfd, buf, n); // 클라이언트에게 응답 전송
        if (total_size + n <= MAX_OBJECT_SIZE) // 캐시 크기 제한 확인
            memcpy(object_buf + total_size, buf, n); // 캐시에 저장하기 위한 버퍼에 응답 데이터 저장
        total_size += n; // 총 크기 업데이트
    }
    Close(serverfd); // end 서버와의 연결 종료

    if (total_size <= MAX_OBJECT_SIZE) {
        pthread_rwlock_wrlock(&cache_lock); // 캐시 락을 쓰기 모드로 잠금
        printf("[STORE] %s (%d bytes)\n", path, total_size); // 캐시에 저장
        cache_insert(path, object_buf, total_size); // 캐시 삽입
        pthread_rwlock_unlock(&cache_lock); // 캐시 삽입 후 쓰기 락 해제
    }
}

int parse_uri(char *uri, char *host, char *port, char *path)
{
    char *hostbegin, *pathbegin, *portbegin; // 호스트, 경로, 포트 시작 위치를 저장할 포인터
    if (strncmp(uri, "http://", 7) != 0) return -1; // URI가 http://로 시작하지 않으면 오류
    hostbegin = uri + 7; // URI에서 http:// 부분을 건너뛰고 호스트 시작 위치로 이동
    pathbegin = strchr(hostbegin, '/'); // 호스트와 경로 구분을 위한 '/' 찾기
    if (pathbegin) { // 경로가 존재하는 경우
        strcpy(path, pathbegin); // 경로 시작 위치를 path에 복사
        *pathbegin = '\0'; // '/'를 NULL로 변경하여 호스트와 경로를 분리
    } else strcpy(path, "/"); // 경로가 없는 경우 기본값 '/'로 설정
    portbegin = strchr(hostbegin, ':'); // 호스트와 포트 구분을 위한 ':' 찾기
    if (portbegin) { // 포트가 존재하는 경우
        *portbegin = '\0'; // ':'를 NULL로 변경하여 호스트와 포트를 분리
        strcpy(host, hostbegin); // 호스트 시작 위치를 host에 복사
        strcpy(port, portbegin + 1); // 포트 시작 위치를 port에 복사
    } else { // 포트가 없는 경우 기본값 80으로 설정
        strcpy(host, hostbegin); // 호스트 시작 위치를 host에 복사
        strcpy(port, "80"); // 포트 기본값 80으로 설정
    }
    return 0;
}

void cache_init() {
    pthread_rwlock_init(&cache_lock, NULL); // POSIX 쓰레드 읽기/쓰기 락 초기화
    head = tail = NULL; // 캐시 블록 초기화
    cache_size = 0; // 
}

cache_block *cache_find(const char *uri) {
    cache_block *p = head; // 캐시 블록을 순회하기 위한 포인터
    while (p) { // 캐시 블록을 순회
        if (strcmp(p->uri, uri) == 0) return p; // URI가 일치하는 블록을 찾으면 반환
        p = p->next; // 다음 블록으로 이동
    }
    return NULL;
}

void cache_insert(const char *uri, const char *data, int size) {
    if (size > MAX_OBJECT_SIZE) return; // 캐시 크기 제한 초과 시 삽입하지 않음
    cache_evict(size); // 캐시 크기 조정
    cache_block *blk = malloc(sizeof(cache_block)); // 새로운 캐시 블록 생성
    blk->data = malloc(size); // 데이터 저장을 위한 메모리 할당
    memcpy(blk->data, data, size); // 데이터 복사
    blk->size = size; // 블록 크기 설정
    strcpy(blk->uri, uri); // URI 저장
    blk->prev = NULL; // 이전 블록 포인터 초기화
    blk->next = head; // 다음 블록 포인터 설정
    if (head) head->prev = blk; // 기존 헤드 블록의 이전 포인터를 현재 블록으로 설정
    head = blk; // 새로운 블록을 헤드로 설정 
    if (!tail) tail = blk; // 처음 삽입 시 tail도 현재 블록으로 설정
    cache_size += size; // 캐시 크기 업데이트
}

void cache_evict(int needed_size) {
    while (cache_size + needed_size > MAX_CACHE_SIZE && tail) { // 캐시 크기 조정
        cache_block *old = tail; // 제거할 블록 포인터
        cache_size -= old->size; // 캐시 크기 업데이트
        if (old->prev) old->prev->next = NULL; // 이전 블록의 다음 포인터를 NULL로 설정
        else head = NULL; // 제거할 블록이 헤드인 경우 헤드를 NULL로 설정
        tail = old->prev; // tail을 이전 블록으로 설정
        free(old->data); // 데이터 메모리 해제
        free(old); // 블록 메모리 해제
    }
}