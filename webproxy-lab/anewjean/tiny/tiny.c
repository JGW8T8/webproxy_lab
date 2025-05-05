<<<<<<< HEAD
/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg,
                 char *longmsg);

int main(int argc, char **argv)
{
  int listenfd, connfd;
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]);
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr,
                    &clientlen); // line:netp:tiny:accept
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,
                0);
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);  // line:netp:tiny:doit
    Close(connfd); // line:netp:tiny:close
  }
}
=======
/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd); // 
void read_requesthdrs(rio_t *rp); // 요청 헤더 읽기
int parse_uri(char *uri, char *filename, char *cgiargs); // URI 분석
void serve_static(int fd, char *filename, int filesize, int is_head); // 정적 콘텐츠 제공
void serve_dynamic(int fd, char *filename, char *cgiargs); // 동적 콘텐츠 제공
void get_filetype(char *filename, char *filetype); // 파일 타입 결정
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg); // 클라이언트 오류 처리

FILE *log_file;

int main(int argc, char **argv)
{
  // log_file = fopen("tiny.log", 'a');
  int listenfd, connfd; // 서버 소켓fd와 클라이언트 소켓fd
  char hostname[MAXLINE], port[MAXLINE]; // 클라이언트 호스트명과 포트
  socklen_t clientlen; // 클라이언트 주소 길이
  struct sockaddr_storage clientaddr; // 클라이언트 주소 구조체

  /* Check command line args */
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }

  listenfd = Open_listenfd(argv[1]); // 리스닝 소켓 생성
  while (1)
  {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트의 연결 요청 수락
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0); // 클라이언트 주소 정보 가져오기
    printf("Accepted connection from (%s, %s)\n", hostname, port); // 클라이언트의 연결 정보 출력
    // fprintf(log_file, "Accepted connection from (%s, %s)\n", hostname, port); 
    doit(connfd);  // 클라이언트 요청 처리
    Close(connfd); // 클라이언트 소켓 닫기
    // fflush(log_file);
  }
}

void doit(int fd)
{
  int is_static;
  int is_head;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;
  size_t n;

  // 요청 읽기
  Rio_readinitb(&rio, fd);
  if (!(Rio_readlineb(&rio, buf, MAXLINE))) return; // 요청 라인이 없으면 종료
  printf(buf); // 요청 라인 에코
  printf("Request headers: \n");
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET") != 0 && strcasecmp(method, "HEAD") != 0) { // method != GET 또는 method != HEAD이면 에러
    clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
    return;                                                                                  
  }

  if (strcasecmp(method, "HEAD") == 0)
    is_head = 1;
  else
    is_head = 0;

  read_requesthdrs(&rio); // 요청 헤더 읽기

  // 요청에서 받은 URI 분석 
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0) {
    clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
    return;
  }

  if (is_static) {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { // 파일이 정적이고 읽기 권한이 없으면
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size, is_head);
  }

  else {
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { // 파일이 동적이고 실행 권한이 없으면
      clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
  char buf[MAXLINE], body[MAXBUF];

  // HTTP 응답 바디 생성
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  // HTTP 응답 출력
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}

void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  while(strcmp(buf, "\r\n")) {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

int parse_uri(char *uri, char *filename, char *cgiargs) 
{
  char *ptr;
  // 정적 콘텐츠
  if (!strstr(uri, "cgi-bin")) { // 'cgi-bin'이 없으면 정적 콘텐츠
    strcpy(cgiargs, ""); 
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri)-1] == '/') {
      strcat(filename, "home.html");
    }
    return 1;
  }
  // 동적 콘텐츠
  else {
    ptr = index(uri, '?'); // '?'의 위치 찾기
    if (ptr) { // '?'가 있으면
      strcpy(cgiargs, ptr+1); // '?' 다음의 문자열을 cgiargs에 복사
      *ptr = '\0'; // '?'를 '\0'으로 바꿔서 ? 기점으로 URI 나누기
    }
    else {
      strcpy(cgiargs, ""); // '?'가 없으면 cgiargs는 빈 문자열
    }
    strcpy(filename, "."); // filenamae = "." 으로 초기화
    strcat(filename, uri); // filename에 uri를 붙임 -> ./uri
    return 0;
  }
}

void serve_static(int fd, char *filename, int filesize, int is_head)
{
  int srcfd;
  char *srcp, filetype[MAXLINE], buf[MAXBUF];

  // 클라이언트에게 응답 헤더 전송
  get_filetype(filename, filetype);
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
  sprintf(buf, "%sConnection: close\r\n", buf);
  sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
  sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers: \n");
  printf("%s", buf);

  if (is_head) // HEAD 요청이면 응답 바디 전송 안함
    return;

  // 클라이언트에게 응답 바디 전송
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize); 
}

void get_filetype(char *filename, char *filetype) 
{
  if (strstr(filename, ".html")) 
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif")) 
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".jpg")) 
    strcpy(filetype, "image/jpeg");
  else if (strstr(filename, ".png")) 
    strcpy(filetype, "image/png");
  else
    strcpy(filetype, "text/plain");
}

void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
  char buf[MAXLINE], *emptylist[] = { NULL };

  // 클라이언트에게 HTTP 응답의 첫 번째 부분 반환
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { // 자식 프로세스
    setenv("QUERY_STRING", cgiargs, 1);
    Dup2(fd, STDOUT_FILENO); // 클라이언트에게 표준 출력 리다이렉트
    Execve(filename, emptylist, environ); // CGI 프로그램 실행
  }
  Wait(NULL); // 자식 프로세스 종료 대기
}
>>>>>>> 6252864af1d9b5dd381a920716b7c7766641a1ab
