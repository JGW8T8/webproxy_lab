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

