#include <stdio.h>
#include "csapp.h"

// int open_clientfd(char *hostname, char*port);

int main(int argc, char **argv) {
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

    Rio_readinitb(&rio, clientfd);

   
    while (Fgets(buf, MAXLINE, stdin) != NULL) {

        Rio_writen(clientfd, buf, strlen(buf));

        Rio_readlineb(&rio, buf, MAXLINE);

        Fputs(buf, stdout);
    }

    Close(clientfd);
    exit(0);
}

// int open_clientfd(char *hostname, char*port) {
   
//     int clientfd;
//     struct addrinfo hints, *listp, *p;

//     memset(&hints, 0, sizeof(struct addrinfo));
//     hints.ai_socktype = SOCK_STREAM;

//     hints.ai_flags = AI_NUMERICSERV;

//     hints.ai_flags |= AI_ADDRCONFIG;

//     Getaddrinfo(hostname, port, &hints, &listp);

//     for(p = listp; p; p = p->ai_next){
//         if((clientfd = socket(p ->ai_family, p->ai_socktype, p->ai_protocol)) < 0)
//         continue;

//         if(connect(clientfd, p->ai_addr, p->ai_addrlen) != -1)
//         break;

//         Close(clientfd);
//     }

//     Freeaddrinfo(listp);

//     if(p == NULL){
//     return -1;
//     }

//     return clientfd;
// }