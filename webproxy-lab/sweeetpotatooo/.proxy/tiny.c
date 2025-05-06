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

/*
 * doit - handle one HTTP request/response transaction
 */
void doit(int fd)
{
  int is_static;
  struct stat sbuf;
  char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
  char filename[MAXLINE], cgiargs[MAXLINE];
  rio_t rio;

  /* Read request line and headers */
  Rio_readinitb(&rio, fd);
  if (!Rio_readlineb(&rio, buf, MAXLINE))
    return;
  printf("%s", buf);
  sscanf(buf, "%s %s %s", method, uri, version);
  if (strcasecmp(method, "GET"))
  {
    clienterror(fd, method, "501", "Not Implemented",
                "Tiny does not implement this method");
    return;
  }
  read_requesthdrs(&rio);

  /* Parse URI from GET request */
  is_static = parse_uri(uri, filename, cgiargs);
  if (stat(filename, &sbuf) < 0)
  {
    clienterror(fd, filename, "404", "Not found",
                "Tiny couldn't find this file");
    return;
  }

  if (is_static)
  { /* Serve static content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't read the file");
      return;
    }
    serve_static(fd, filename, sbuf.st_size);
  }
  else
  { /* Serve dynamic content */
    if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode))
    {
      clienterror(fd, filename, "403", "Forbidden",
                  "Tiny couldn't run the CGI program");
      return;
    }
    serve_dynamic(fd, filename, cgiargs);
  }
}

/*
 * read_requesthdrs - read HTTP request headers
 */
void read_requesthdrs(rio_t *rp)
{
  char buf[MAXLINE];

  Rio_readlineb(rp, buf, MAXLINE);
  printf("%s", buf);
  while (strcmp(buf, "\r\n"))
  {
    Rio_readlineb(rp, buf, MAXLINE);
    printf("%s", buf);
  }
  return;
}

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 */
int parse_uri(char *uri, char *filename, char *cgiargs)
{
  char *ptr;

  if (!strstr(uri, "cgi-bin"))
  { /* Static content */
    strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    if (uri[strlen(uri) - 1] == '/')
      strcat(filename, "home.html");
    return 1;
  }
  else
  { /* Dynamic content */
    ptr = index(uri, '?');
    if (ptr)
    {
      strcpy(cgiargs, ptr + 1);
      *ptr = '\0';
    }
    else
      strcpy(cgiargs, "");
    strcpy(filename, ".");
    strcat(filename, uri);
    return 0;
  }
}

/*
 * serve_static - copy a file back to the client
 */
void serve_static(int fd, char *filename, int filesize)
{
  int srcfd;
  char *srcp, filetype[MAXLINE];

  char buf[MAXBUF];
  char *p = buf;
  int n;
  int remaining = sizeof(buf);

  /* Send response headers to client */
  get_filetype(filename, filetype);

  /* Build the HTTP response headers correctly - use separate buffers or append */
  n = snprintf(p, remaining, "HTTP/1.0 200 OK\r\n");
  p += n;
  remaining -= n;

  n = snprintf(p, remaining, "Server: Tiny Web Server\r\n");
  p += n;
  remaining -= n;

  n = snprintf(p, remaining, "Connection: close\r\n");
  p += n;
  remaining -= n;

  n = snprintf(p, remaining, "Content-length: %d\r\n", filesize);
  p += n;
  remaining -= n;

  n = snprintf(p, remaining, "Content-type: %s\r\n\r\n", filetype);
  p += n;
  remaining -= n;

  Rio_writen(fd, buf, strlen(buf));
  printf("Response headers:\n");
  printf("%s", buf);

  /* Send response body to client */
  srcfd = Open(filename, O_RDONLY, 0);
  srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
  Close(srcfd);
  Rio_writen(fd, srcp, filesize);
  Munmap(srcp, filesize);
}

/*
 * get_filetype - derive file type from file name
 */
void get_filetype(char *filename, char *filetype)
{
  if (strstr(filename, ".html"))
    strcpy(filetype, "text/html");
  else if (strstr(filename, ".gif"))
    strcpy(filetype, "image/gif");
  else if (strstr(filename, ".png"))
    strcpy(filetype, "image/png");
  else if (strstr(filename, ".jpg"))
    strcpy(filetype, "image/jpeg");
  else
    strcpy(filetype, "text/plain");
}

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
  char buf[MAXLINE], *emptylist[] = {NULL};
  pid_t pid;

  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  /* Create a child process to handle the CGI program */
  if ((pid = Fork()) < 0)
  { /* Fork failed */
    perror("Fork failed");
    return;
  }

  if (pid == 0)
  { /* Child process */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);

    /* Redirect stdout to client */
    if (Dup2(fd, STDOUT_FILENO) < 0)
    {
      perror("Dup2 error");
      exit(1);
    }
    Close(fd);

    /* Run CGI program */
    Execve(filename, emptylist, environ);

    /* If we get here, Execve failed */
    perror("Execve error");
    exit(1);
  }
  else
  { /* Parent process */
    /* Parent waits for child to terminate */
    int status;
    if (waitpid(pid, &status, 0) < 0)
    {
      perror("Wait error");
    }

    printf("Child process %d terminated with status %d\n", pid, status);
    /* Parent continues normally - returns to doit() */
  }
  /* When we return from here, doit() will close the connection */
}

/*
 * clienterror - returns an error message to the client
 */
void clienterror(int fd, char *cause, char *errnum,
                 char *shortmsg, char *longmsg)
{
  char buf[MAXLINE], body[MAXBUF];

  /* Build the HTTP response body */
  sprintf(body, "<html><title>Tiny Error</title>");
  sprintf(body, "%s<body bgcolor="
                "ffffff"
                ">\r\n",
          body);
  sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
  sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
  sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

  /* Print the HTTP response */
  sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-type: text/html\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
  Rio_writen(fd, buf, strlen(buf));
  Rio_writen(fd, body, strlen(body));
}
=======
/* tiny.c - A simple, iterative HTTP/1.0 Web server */

#include "csapp.h"

// 함수 선언
void doit(int fd);                                                                  // 요청 처리
void read_requesthdrs(rio_t *rp);                                                   // 요청 헤더 읽기
int parse_uri(char *uri, char *filename, char *cgiargs);                            // URI 파싱
void serve_static(int fd, char *filename, int filesize,int is_head);                // 정적 콘텐츠 처리
void get_filetype(char *filename, char *filetype);                                  // MIME 타입 결정
void serve_dynamic(int fd, char *filename, char *cgiargs);                          // 동적 콘텐츠 처리
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg); // 에러 응답

// 클라이언트 요청 처리 함수
void doit(int fd)
{
    int is_head = 0;
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    // 요청 라인 읽기
    Rio_readinitb(&rio, fd);
    Rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version); // 메서드, URI, 버전 추출
    
    //HEAD 메서드 처리
    if (!strcasecmp(method, "HEAD")) {
        is_head = 1;
    }

    // GET 메서드처리
    else if (strcasecmp(method, "GET")) {
        clienterror(fd, method, "501", "Not implemented", "Tiny does not implement this method");
        return;
    }

    // 요청 헤더 무시하며 읽기
    read_requesthdrs(&rio);

    // URI 파싱 → filename, cgiargs 설정
    is_static = parse_uri(uri, filename, cgiargs);
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }

    // 정적 콘텐츠인지 동적 콘텐츠인지에 따라 분기
    if (is_static) {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
        }
        serve_static(fd, filename, sbuf.st_size, is_head);
    } else {
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }
        serve_dynamic(fd, filename, cgiargs);
    }
}

// 에러 메시지 HTML 생성 및 전송
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    // HTML 본문 작성
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body + strlen(body), "<body bgcolor=\"ffffff\">\r\n");
    sprintf(body + strlen(body), "%s: %s\r\n", errnum, shortmsg);
    sprintf(body + strlen(body), "<p>%s: %s\r\n", longmsg, cause);
    sprintf(body + strlen(body), "<hr><em>The Tiny Web server</em>\r\n");

    // 응답 헤더 전송
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %lu\r\n\r\n", strlen(body));
    Rio_writen(fd, buf, strlen(buf));

    // 본문 전송
    Rio_writen(fd, body, strlen(body));
}

// 요청 헤더 무시하며 출력 (디버깅용)
void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];
    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        printf("%s", buf);
        rio_readlineb(rp, buf, MAXLINE);
    }
    return;
}

// URI 분석 → 정적 or 동적 판단 + filename, cgiargs 추출
int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {
        strcpy(cgiargs, "");
        sprintf(filename, ".%s", uri);
        if (uri[strlen(uri) - 1] == '/')
            strcat(filename, "home.html"); // 기본 페이지 지정
        return 1; // 정적
    } else {
        ptr = index(uri, '?');
        if (ptr) {
            strcpy(cgiargs, ptr + 1);
            *ptr = '\0'; // ? 제거
        } else {
            strcpy(cgiargs, "");
        }
        sprintf(filename, ".%s", uri);
        return 0; // 동적
    }
}

// 정적 콘텐츠 처리
// void serve_static(int fd, char *filename, int filesize)
// {
//     int srcfd;
//     char *srcp, filetype[MAXLINE], buf[MAXBUF];

//     // MIME 타입 결정 및 응답 헤더 생성
//     get_filetype(filename, filetype);
//     sprintf(buf, "HTTP/1.0 200 OK\r\n");
//     sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
//     sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
//     sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);
//     Rio_writen(fd, buf, strlen(buf));

//     // 파일을 메모리에 매핑 후 전송
//     srcfd = Open(filename, O_RDONLY, 0);
//     srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
//     Close(srcfd);
//     Rio_writen(fd, srcp, filesize);
//     Munmap(srcp, filesize);
// }


// 정적 콘텐츠 처리 malloc & rio_readn
void serve_static(int fd, char *filename, int filesize, int is_head)
{
    int srcfd;
    char *srcbuf = malloc(filesize);
    char filetype[MAXLINE], buf[MAXBUF];

    get_filetype(filename, filetype);
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf + strlen(buf), "Server: Tiny Web Server\r\n");
    sprintf(buf + strlen(buf), "Content-length: %d\r\n", filesize);
    sprintf(buf + strlen(buf), "Content-type: %s\r\n\r\n", filetype);
    Rio_writen(fd, buf, strlen(buf));

    if (!is_head) {
        srcfd = Open(filename, O_RDONLY, 0);
        Rio_readn(srcfd, srcbuf, filesize);
        Close(srcfd);
        Rio_writen(fd, srcbuf, filesize);
    }
}



// MIME 타입 결정
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))
        strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))
        strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
        strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
        strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mpg"))
        strcpy(filetype, "video/mpeg");
    
    else
        strcpy(filetype, "text/plain");
}

// 동적 콘텐츠 처리 (CGI 실행)
void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    // 기본 헤더 전송
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));

    if (Fork() == 0) { // 자식 프로세스
        setenv("QUERY_STRING", cgiargs, 1);
        dup2(fd, STDOUT_FILENO); // 표준 출력 → 클라이언트 소켓
        execve(filename, emptylist, environ); // CGI 실행
    }
    Wait(NULL); // 부모는 자식 종료 기다림
}

// 메인 함수: 서버 루프
int main(int argc, char **argv)
{
    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // 리스닝 소켓 열기
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        doit(connfd);  // 요청 처리
        Close(connfd); // 연결 종료
    }
}
>>>>>>> 6252864af1d9b5dd381a920716b7c7766641a1ab
