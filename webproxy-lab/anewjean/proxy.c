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
    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    cache_init();
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, thread, connfdp);
    }
}

void *thread(void *vargp)
{
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self());
    free(vargp);

    forward_request(connfd);
    Close(connfd);
    return NULL;
}

void forward_request(int connfd)
{
    rio_t client_rio, server_rio;
    char buf[MAXLINE], req[MAX_OBJECT_SIZE];
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char host[MAXLINE], port[10], path[MAXLINE];

    Rio_readinitb(&client_rio, connfd);
    if (!Rio_readlineb(&client_rio, buf, MAXLINE)) return;
    sscanf(buf, "%s %s %s", method, uri, version);

    if (strcasecmp(method, "GET") != 0) return;
    if (parse_uri(uri, host, port, path) < 0) return;

    printf("[LOOKUP] %s\n", path);
    pthread_rwlock_rdlock(&cache_lock);
    cache_block *cb = cache_find(path);
    if (cb) {
        printf("[HIT] %s\n", path);
        Rio_writen(connfd, cb->data, cb->size);
        pthread_rwlock_unlock(&cache_lock);
        return;
    }
    pthread_rwlock_unlock(&cache_lock);

    sprintf(req, "GET %s HTTP/1.0\r\n", path);
    while (Rio_readlineb(&client_rio, buf, MAXLINE) > 0 && strcmp(buf, "\r\n") != 0) {
        if (strncasecmp(buf, "Host:", 5) == 0 ||
            strncasecmp(buf, "User-Agent:", 11) == 0 ||
            strncasecmp(buf, "Connection:", 11) == 0 ||
            strncasecmp(buf, "Proxy-Connection:", 17) == 0)
            continue;
        strcat(req, buf);
    }
    sprintf(buf, "Host: %s\r\n", host); strcat(req, buf);
    strcat(req, user_agent_hdr);
    strcat(req, "Connection: close\r\n");
    strcat(req, "Proxy-Connection: close\r\n\r\n");

    int serverfd = Open_clientfd(host, port);
    if (serverfd < 0) return;

    Rio_readinitb(&server_rio, serverfd);
    Rio_writen(serverfd, req, strlen(req));

    char object_buf[MAX_OBJECT_SIZE];
    int total_size = 0, n;
    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0) {
        Rio_writen(connfd, buf, n);
        if (total_size + n <= MAX_OBJECT_SIZE)
            memcpy(object_buf + total_size, buf, n);
        total_size += n;
    }
    Close(serverfd);

    if (total_size <= MAX_OBJECT_SIZE) {
        pthread_rwlock_wrlock(&cache_lock);
        printf("[STORE] %s (%d bytes)\n", path, total_size);
        cache_insert(path, object_buf, total_size);
        pthread_rwlock_unlock(&cache_lock);
    }
}

int parse_uri(char *uri, char *host, char *port, char *path)
{
    char *hostbegin, *pathbegin, *portbegin;
    if (strncmp(uri, "http://", 7) != 0) return -1;
    hostbegin = uri + 7;
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin) {
        strcpy(path, pathbegin);
        *pathbegin = '\0';
    } else strcpy(path, "/");
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

void cache_init() {
    pthread_rwlock_init(&cache_lock, NULL);
    head = tail = NULL;
    cache_size = 0;
}

cache_block *cache_find(const char *uri) {
    cache_block *p = head;
    while (p) {
        if (strcmp(p->uri, uri) == 0) return p;
        p = p->next;
    }
    return NULL;
}

void cache_insert(const char *uri, const char *data, int size) {
    if (size > MAX_OBJECT_SIZE) return;
    cache_evict(size);
    cache_block *blk = malloc(sizeof(cache_block));
    blk->data = malloc(size);
    memcpy(blk->data, data, size);
    blk->size = size;
    strcpy(blk->uri, uri);
    blk->prev = NULL;
    blk->next = head;
    if (head) head->prev = blk;
    head = blk;
    if (!tail) tail = blk;
    cache_size += size;
}

void cache_evict(int needed_size) {
    while (cache_size + needed_size > MAX_CACHE_SIZE && tail) {
        cache_block *old = tail;
        cache_size -= old->size;
        if (old->prev) old->prev->next = NULL;
        else head = NULL;
        tail = old->prev;
        free(old->data);
        free(old);
    }
}