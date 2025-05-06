#include <stdio.h>
#include <signal.h>

#include "csapp.h"
#include "cache.h"

// 최대 캐시 크기 및 한 객체의 최대 크기
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

// 함수 선언
void *thread(void *vargp);  // 스레드 함수
void doit(int clientfd);    // 요청을 처리 메인 함수
void read_requesthdrs(rio_t *rp, void *buf, int serverfd, char *hostname, char *port); // 요청 헤더 처리
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);    // 에러 응답 생성
void parse_uri(char *uri, char *hostname, char *port, char *path);                     // URI 파싱

// 고정된 User-Agent 헤더 (프록시가 이 값을 사용)
static const int is_local_test = 1;
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";


int main(int argc, char **argv)
{
  int listenfd, *clientfd;
  char client_hostname[MAXLINE], client_port[MAXLINE];
  socklen_t clientlen;                                  // 주소 길이
  struct sockaddr_storage clientaddr;                   // 클라이언트 주소 정보 구조체
  pthread_t tid;                                        // 스레드 ID

  signal(SIGPIPE, SIG_IGN); // 클라이언트 종료 시 SIGPIPE 무시 (서버 죽지 않게)

  // 캐시 리스트 초기화 (전역)
  rootp = (CachedObject *)calloc(1, sizeof(CachedObject)); // 캐시 맨 앞
  lastp = (CachedObject *)calloc(1, sizeof(CachedObject)); // 캐시 맨 뒤

  //실행파일 + 포트번호 없으면 에러
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  // 프록시 서버 리스닝 소켓 열기 => socket() -> bind( ) -> listen()
  listenfd = Open_listenfd(argv[1]);

  // while (1) {
  //   1. 누가 접속하면 -> 그 연결을 accept
  //   2. 그 연결 정보를 담은 소켓을 메모리에 동적 할당
  //   3. 새 스레드를 하나 만들고 -> 거기서 클라이언트 요청을 처리한다
  // }

  while (1)
  {

    clientlen = sizeof(clientaddr);   // 클라이언트 요청 수신, Accept함수에 주소크기 넘기기 위해
    clientfd = Malloc(sizeof(int));  // 클라이언트 소켓을 동적 할당
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //클라이언트 연결 수락
    //listenfd: 대기소켓
    //Accept(): 클라이언트 통신 소켓번호 반환


    // 클라이언트 주소 출력
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);

    // 요청을 독립 스레드 생성 및 처리
    Pthread_create(&tid, NULL, thread, clientfd);
  }
}

// 스레드 함수
void *thread(void *vargp)
{
  int clientfd = *((int *)vargp);       // 전달받은 client 소켓
  Pthread_detach(pthread_self());       // 스레드 종료 시 자원 자동 회수
  Free(vargp);                          // 힙에 할당한 clientfd 포인터 해제
  doit(clientfd);                       // 요청 처리
  Close(clientfd);                      // 클라이언트 연결 종료
  return NULL;
}

// 한 줄의 헤더를 받아서 Proxy가 요구하는 형식에 맞게 수정하거나 그대로 전달
int handle_header_line(char *line, char *write_buf, int *host, int *conn, int *proxy_conn, int *user_agent) {
  
  // Proxy-Connection 헤더 발견 → 무조건 close로 고정
  if (strstr(line, "Proxy-Connection")) {
    sprintf(write_buf, "Proxy-Connection: close\r\n");
    *proxy_conn = 1;  // 존재 표시
  }

  // Connection 헤더 발견 → close로 고정
  else if (strstr(line, "Connection")) {
    sprintf(write_buf, "Connection: close\r\n");
    *conn = 1;
  }

  // User-Agent 헤더 발견 → 고정된 문자열로 교체
  else if (strstr(line, "User-Agent")) {
    sprintf(write_buf, "%s", user_agent_hdr);
    *user_agent = 1;
  }

  // Host 헤더 발견 → 수정하지 않고 그대로 전달
  else if (strstr(line, "Host")) {
    *host = 1;
    strcpy(write_buf, line);
  }

  // 그 외 일반 헤더는 그대로 전달
  else {
    strcpy(write_buf, line);
  }

  // write_buf에 들어간 문자열 길이 반환
  return strlen(write_buf);
}
// 필수 헤더가 없을 경우 기본값으로 보충해서 서버에 전달
void add_missing_headers(int serverfd, int host, int conn, int proxy_conn, int user_agent, char *hostname, char *port) {
  char buf[MAXLINE];

  // Proxy-Connection 헤더가 없었다면 추가
  if (!proxy_conn) {
    sprintf(buf, "Proxy-Connection: close\r\n");
    Rio_writen(serverfd, buf, strlen(buf));
  }

  // Connection 헤더가 없었다면 추가
  if (!conn) {
    sprintf(buf, "Connection: close\r\n");
    Rio_writen(serverfd, buf, strlen(buf));
  }

  // Host 헤더가 없었다면 hostname, port를 사용해 추가
  if (!host) {
    if (!is_local_test)
      hostname = "52.79.234.188";  // AWS 예외 처리
    sprintf(buf, "Host: %s:%s\r\n", hostname, port);
    Rio_writen(serverfd, buf, strlen(buf));
  }

  // User-Agent 헤더가 없었다면 고정된 문자열로 추가
  if (!user_agent) {
    sprintf(buf, "%s", user_agent_hdr);
    Rio_writen(serverfd, buf, strlen(buf));
  }

  // 헤더 끝을 알리는 빈 줄
  sprintf(buf, "\r\n");
  Rio_writen(serverfd, buf, strlen(buf));
}


// 클라이언트가 보낸 요청 헤더를 읽고, 서버로 전달할 형태로 정리해서 전송
void read_requesthdrs(rio_t *request_rio, void *request_buf, int serverfd, char *hostname, char *port)
{
  // 각 필수 헤더의 존재 여부를 추적할 변수들 초기화
  int is_host_exist = 0;
  int is_conn_exist = 0;
  int is_proxy_conn_exist = 0;
  int is_user_agent_exist = 0;

  // 첫 번째 줄은 doit() 함수에서 이미 읽었기 때문에 생략 가능
  Rio_readlineb(request_rio, request_buf, MAXLINE);

  // 빈 줄("\r\n") 전까지 반복해서 헤더 읽기
  while (strcmp(request_buf, "\r\n")) {

    // 서버에 전송할 헤더를 저장할 버퍼
    char write_buf[MAXLINE];

    // 현재 헤더 라인을 처리하고, 필수 헤더 존재 여부 체크
    handle_header_line(request_buf, write_buf,&is_host_exist, &is_conn_exist,&is_proxy_conn_exist, &is_user_agent_exist);

    // 처리된 헤더를 서버로 전송
    Rio_writen(serverfd, write_buf, strlen(write_buf));

    // 다음 줄 읽기
    Rio_readlineb(request_rio, request_buf, MAXLINE);
  }

  // 누락된 필수 헤더가 있으면 보충해서 서버로 전송
  add_missing_headers(serverfd,is_host_exist, is_conn_exist,is_proxy_conn_exist, is_user_agent_exist,hostname, port);
}


void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  // 에러 Bdoy 생성
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // 에러 Header 생성 & 전송
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));

  // 에러 Body 전송
  Rio_writen(fd, body, strlen(body));
}


// 요청 처리 함수
void doit(int clientfd)
{
  int serverfd, content_length;
  char request_buf[MAXLINE], response_buf[MAXLINE];
  char method[MAXLINE], uri[MAXLINE], path[MAXLINE], hostname[MAXLINE], port[MAXLINE];
  char *response_ptr;
  rio_t request_rio, response_rio;

  // 클라이언트 요청 읽기
  Rio_readinitb(&request_rio, clientfd);
  Rio_readlineb(&request_rio, request_buf, MAXLINE);
  printf("Request headers:\n %s\n", request_buf);

  // method, uri 추출 → uri 파싱
  sscanf(request_buf, "%s %s", method, uri);
  parse_uri(uri, hostname, port, path);

  // 지원하지 않는 method 예외 처리
  if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD"))
  {
    clienterror(clientfd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;
  }

  // 캐시 확인 (LRU 캐시 정책 사용)
  //LRU (Least Recently Used): 가장 오래전에 사용된 데이터를 가장 먼저 제거한다
  CachedObject *cached_object = find_cache(path);
  if (cached_object)
  {
    send_cache(cached_object, clientfd); // 클라이언트에게 캐시 전송
    read_cache(cached_object);           // LRU 갱신
    return;
  }

  // 서버 연결
  serverfd = is_local_test ? Open_clientfd(hostname, port) : Open_clientfd("52.79.234.188", port);
  if (serverfd < 0)
  {
    clienterror(serverfd, method, "502", "Bad Gateway", "Failed to establish connection with the end server");
    return;
  }

  // 첫 줄 재구성 후 서버로 전송
  sprintf(request_buf, "%s %s HTTP/1.0\r\n", method, path);
  Rio_writen(serverfd, request_buf, strlen(request_buf));

  // 요청 헤더 처리
  read_requesthdrs(&request_rio, request_buf, serverfd, hostname, port);

  // 응답 헤더 수신 및 전송
  Rio_readinitb(&response_rio, serverfd);
  while (strcmp(response_buf, "\r\n"))
  {
    Rio_readlineb(&response_rio, response_buf, MAXLINE);
    if (strstr(response_buf, "Content-length"))
      content_length = atoi(strchr(response_buf, ':') + 1);
    Rio_writen(clientfd, response_buf, strlen(response_buf));
  }

  // 응답 본문 수신 및 전송
  response_ptr = malloc(content_length);
  Rio_readnb(&response_rio, response_ptr, content_length);
  Rio_writen(clientfd, response_ptr, content_length);

  // 캐싱 가능한 경우 캐시에 저장
  if (content_length <= MAX_OBJECT_SIZE)
  {
    CachedObject *Cache = (CachedObject *)calloc(1, sizeof(CachedObject));
    Cache->response_ptr = response_ptr;
    Cache->content_length = content_length;
    strcpy(Cache->path, path);
    write_cache(Cache);
  }
  else
    free(response_ptr);  // 캐싱 안 하는 경우 메모리 해제

  Close(serverfd);
}


// 파싱 함수 ex) http://example.com:8080/index.html
// 네트워크 주소 분리 
void parse_uri(char *uri, char *hostname, char *port, char *path)
{
  char *hostname_ptr = strstr(uri, "//") ? strstr(uri, "//") + 2 : uri;
  char *port_ptr = strchr(hostname_ptr, ':');  // : 뒤는 포트
  char *path_ptr = strchr(hostname_ptr, '/');  // /부터는 경로
  strcpy(path, path_ptr);  // path 복사

  if (port_ptr) {
    // 포트가 명시된 경우: hostname:port/path
    strncpy(port, port_ptr + 1, path_ptr - port_ptr - 1);
    strncpy(hostname, hostname_ptr, port_ptr - hostname_ptr);
  } else {
    // 포트가 없는 경우: 기본 포트 할당
    strcpy(port, is_local_test ? "80" : "8000");
    strncpy(hostname, hostname_ptr, path_ptr - hostname_ptr);
  }
}