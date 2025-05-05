#include "csapp.h"

int main(int argc, char *argv[]) {
    int clientfd;
    char *host, *port, buf[MAXLINE];
    rio_t rio;

    if (argc != 3) {
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
        exit(0);
    }

    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);
    if (clientfd < 0) {
        fprintf(stderr, "Connection failed to %s:%s\n", host, port);
        exit(1);
    }

    Rio_readinitb(&rio, clientfd);

    while (Fgets(buf, MAXLINE, stdin) != NULL) {
        Rio_writen(clientfd, buf, strlen(buf));
        if (Rio_readlineb(&rio, buf, MAXLINE) == 0) {
            fprintf(stderr, "Server terminated connection\n");
            break;
        }
        Fputs(buf, stdout);
    }

    Close(clientfd);
    return 0;
}
