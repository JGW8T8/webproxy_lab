#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

int parse_uri(char *uri, char *host, char *port, char *path);
void func(int connfd);

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main(int argc, char **argv) { //main() 서버 초기화 및 요청 수락
  int listenfd, connfd;
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  char hostname[MAXLINE], port[MAXLINE];

  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);

  while (1) { //지정한 포트에서 클라이언트의 연결 요청을 계속 수락합니다. /연결된 소켓을 func() 함수로 넘겨 요청을 처리합니다.
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //클라이언트 연결 수락
    func(connfd); //요청 처리
    Close(connfd); // 연결 종료
  }
}

void func(int connfd) {
  rio_t client_rio, server_rio;
  char buf[MAXLINE], req[MAX_OBJECT_SIZE];
  char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char host[MAXLINE], port[10], path[MAXLINE];

  Rio_readinitb(&client_rio, connfd);

  if (!Rio_readlineb(&client_rio, buf, MAXLINE)) return;
  sscanf(buf, "%s %s %s", method, uri, version);

  if (parse_uri(uri, host, port, path) == -1) {
    fprintf(stderr, "올바른 URI가 아닙니다: %s\n", uri);
    return;
  }

  sprintf(req, "GET %s HTTP/1.0\r\n", path);

  while (Rio_readlineb(&client_rio, buf, MAXLINE) > 0 && strcmp(buf, "\r\n") != 0) {
    if (strncasecmp(buf, "Host", 4) == 0 ||
        strncasecmp(buf, "User-Agent", 10) == 0 ||
        strncasecmp(buf, "Connection", 10) == 0 ||
        strncasecmp(buf, "Proxy-Connection", 16) == 0) {
      continue;
    }
    strcat(req, buf);
  }

  sprintf(buf, "Host: %s\r\n", host); strcat(req, buf);
  sprintf(buf, "%s", user_agent_hdr); strcat(req, buf);
  sprintf(buf, "Connection: close\r\n"); strcat(req, buf);
  sprintf(buf, "Proxy-Connection: close\r\n\r\n"); strcat(req, buf);

  printf("최종 요청:\n%s\n", req);

  int serverfd = Open_clientfd(host, port);
  if (serverfd < 0) {
    fprintf(stderr, "원 서버 연결 실패\n");
    return;
  }
  Rio_readinitb(&server_rio, serverfd);

  Rio_writen(serverfd, req, strlen(req));

  int n;
  while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) > 0) {
    Rio_writen(connfd, buf, n);
    if (strcmp(buf, "\r\n") == 0) break;
  }

  while ((n = Rio_readnb(&server_rio, buf, MAXBUF)) > 0) {
    Rio_writen(connfd, buf, n);
  }

  Close(serverfd);
}

int parse_uri(char *uri, char *host, char *port, char *path) {
  char *hostbegin, *hostend, *pathbegin, *portbegin;

  if (strncasecmp(uri, "http://", 7) != 0) {
    return -1;
  }

  hostbegin = uri + 7;

  pathbegin = strchr(hostbegin, '/');
  if (pathbegin) {
    strcpy(path, pathbegin);
    *pathbegin = '\0';
  } else {
    strcpy(path, "/");
  }

  portbegin = strchr(hostbegin, ':');
  if (portbegin) {
    *portbegin = '\0';
    strcpy(host, hostbegin);
    strcpy(port, portbegin + 1);
  } else {
    strcpy(host, hostbegin);
    strcpy(port, "80");
  }

  return 0;
}
