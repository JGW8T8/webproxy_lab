<<<<<<< HEAD
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";

int main()
{
  printf("%s", user_agent_hdr);
  return 0;
}
=======
#include <stdio.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

void *thread(void *vargp);
void doit(int clientfd);
void read_requesthdrs(rio_t *rp, void *buf, int serverfd, char *hostname, char *port);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void parse_uri(char *uri, char *hostname, char *port, char *path);

/* You won't lose style points for including this long line in your code */

// 테스트 환경에 따른 도메인&포트 지정을 위한 상수 (0 할당 시 도메인&포트가 고정되어 외부에서 접속 가능)
static const int is_local_test = 1; 
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 "
    "Firefox/10.0.3\r\n";


int main(int argc, char **argv)
{
  int listenfd, *clientfd;
  char client_hostname[MAXLINE], client_port[MAXLINE];
  socklen_t clientlen;
  struct sockaddr_storage clientaddr;
  pthread_t tid;
  signal(SIGPIPE, SIG_IGN); // SIGPIPE 예외처리
    
  rootp = (web_object_t *)calloc(1, sizeof(web_object_t));
  lastp = (web_object_t *)calloc(1, sizeof(web_object_t));
    
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
    
  listenfd = Open_listenfd(argv[1]); // 전달받은 포트 번호를 사용해 수신 소켓 생성
  while (1)
  {
    clientlen = sizeof(clientaddr);
    clientfd = Malloc(sizeof(int));
    *clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); // 클라이언트 연결 요청 수신
    Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
    printf("Accepted connection from (%s, %s)\n", client_hostname, client_port);
    Pthread_create(&tid, NULL, thread, clientfd); // Concurrent 프록시
  }
}

void *thread(void *vargp)
{
  int clientfd = *((int *)vargp);
  Pthread_detach(pthread_self());
  Free(vargp);
  doit(clientfd);
  Close(clientfd);
  return NULL;
}

>>>>>>> 6252864af1d9b5dd381a920716b7c7766641a1ab
