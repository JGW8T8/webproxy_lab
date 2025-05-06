#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "csapp.h"

#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

void *thread(void *vargp);
void forward_request(int connfd);
int parse_uri(char *uri, char *host, char *port, char *path);

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

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, thread, connfdp);
    }
    return 0;
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

    if (strcasecmp(method, "GET") != 0) {
        fprintf(stderr, "Proxy only supports GET\n");
        return;
    }

    if (parse_uri(uri, host, port, path) < 0) {
        fprintf(stderr, "Invalid URI\n");
        return;
    }

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
    if (serverfd < 0) {
        fprintf(stderr, "Failed to connect to end server\n");
        return;
    }

    Rio_readinitb(&server_rio, serverfd);
    Rio_writen(serverfd, req, strlen(req));

    int n;
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) > 0)
        Rio_writen(connfd, buf, n);

    while ((n = Rio_readnb(&server_rio, buf, MAXLINE)) > 0)
        Rio_writen(connfd, buf, n);

    Close(serverfd);
}

int parse_uri(char *uri, char *host, char *port, char *path)
{
    char *hostbegin, *hostend, *pathbegin, *portbegin;

    if (strncmp(uri, "http://", 7) != 0) return -1;
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
