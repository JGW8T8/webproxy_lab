#include <stdio.h>
#include "csapp.h"

/* 캐시 최대 크기 */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv);
// thread(void *vargp);
// handle_client(int fd);
int parse_uri(char *uri, char *host, char *port, char *path);
// format_http_header();
// cache_init();
// cache_find(uri, buf);
// cache_store(urio, data);
// cache_evict();
// reader_writer_lock();
void forward_request(int connfd);

int main(int argc, char **argv)
{
  printf("%s", user_agent_hdr);
  int listenfd, connfd;
  __socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  
  while (1) {
    // 1. 프록시 서버 listen
    // struct sockaddr_storage clientaddr;
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    forward_request(connfd);
    Close(connfd);
  }
  // return 0;
}

void forward_request(int connfd)  
{
  rio_t client_rio, server_rio;
  char buf[MAXLINE], req[MAX_OBJECT_SIZE];
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[10], path[MAXLINE];

  // 버퍼 초기화
  Rio_readinitb(&client_rio, connfd);

  // 요청 읽기
  if (!Rio_readlineb(&client_rio, buf, MAXLINE)) return;
  sscanf(buf, "%s %s %s", method, uri, version); // 요청 메소드, URI, 버전 분리해서 읽기

  // uri 파싱 -> host, port, path 추출
  if (parse_uri(uri, host, port, path) == -1) {
    fprintf(stderr, "Invalid URI\n");
    return;
  }

  // 2. 요청 라인 생성
  sprintf(req, "GET %s HTTP/1.0\r\n", path);

  // 3. 요청 헤더 생성
  while (Rio_readlineb(&client_rio, buf, MAXLINE) > 0 && strcmp(buf, "\r\n") != 0) {
    // if (strcmp(buf, "\r\n") == 0) break;
    if (strncasecmp(buf, "Host:", 4) == 0 ||
        strncasecmp(buf, "User-Agent:", 11) == 0 ||
        strncasecmp(buf, "Connection:", 11) == 0 ||
        strncasecmp(buf, "Proxy-Connection:", 17) == 0) {
      continue;
    }
    strcat(req, buf); // 나머지 헤더는 그대로 요청에 추가
  }

  // 4. 필수 표준 헤더 수동으로 추가
  sprintf(buf, "Host: %s\r\n", host); strcat(req, buf); // User-Agent 헤더 추가
  sprintf(buf, "%s", user_agent_hdr); strcat(req, buf); // Connection 헤더 추가
  sprintf(buf, "Connection: close\r\n"); strcat(req, buf); // Proxy-Connection 헤더 추가
  sprintf(buf, "Proxy-Connection: close\r\n\r\n"); strcat(req, buf); // 요청 끝

  // 5. 원 서버에 연결
  int serverfd = Open_clientfd(host, port);
  if (serverfd < 0) {
    fprintf(stderr, "Failed to connect to server\n");
    return;
  }

  Rio_readinitb(&server_rio, serverfd); // 서버 소켓 초기화

  // 6. 요청 전송
  Rio_writen(serverfd, req, strlen(req));

  // 7. 응답 헤더를 클라이언트에게 전달
  int n;
  while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) > 0) {
    Rio_writen(connfd, buf, n);
    if (strcmp(buf, "\r\n") == 0) break;
  }

  // 8. 응답 바디를 클라이언트에게 전달
  while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
    Rio_writen(connfd, buf, n);
  }

  Close(serverfd); // 서버 소켓 닫기
}

int parse_uri(char *uri, char *host, char *port, char *path) {
  char *hostbegin, *hostend, *pathbegin, *portbegin;

  // uri 앞 7글자가 "http://"로 시작하는지 확인
  if (strncmp(uri, "http://", 7) != 0) return -1;

  hostbegin = uri + 7;

  pathbegin = strchr(hostbegin, '/');
  if (pathbegin) {
    strcpy(path, pathbegin); // path 복사
    *pathbegin = '\0'; // host 문자열 종료 지점 설정
  } 
  else {
    strcpy(path, "/"); // path가 없으면 기본값으로 "/"
  }

  // ':'가 있으면 포트 처리
  portbegin = strchr(hostbegin, ':');
  if (portbegin) {
    *portbegin = '\0'; // host 문자열 종료 지점 설정
    strcpy(host, hostbegin); // 포트 복사
    strcpy(port, portbegin + 1); // 포트 복사
  } 
  else {
    strcpy(host, hostbegin); // host 복사
    strcpy(port, "80"); // 기본 포트 80
  }

  return 0;
}
