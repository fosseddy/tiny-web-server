#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#define HOSTCAP 255
#define PORTCAP 6

int open_listensock(char *port)
{
    struct addrinfo *info, *addr, hints;
    int rc, sockfd, sockopt_val = 1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_ADDRCONFIG;

    rc = getaddrinfo(NULL, port, &hints, &info);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo failed: %s\n", gai_strerror(rc));
        exit(1);
    }

    for (addr = info; addr != NULL; addr = addr->ai_next) {
        sockfd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (sockfd < 0) continue;

        rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
                        &sockopt_val, sizeof(int));
        if (rc < 0) {
            perror("setsockopt failed");
            exit(1);
        }

        rc = bind(sockfd, addr->ai_addr, addr->ai_addrlen);
        if (rc == 0) break;

        close(sockfd);
        sockfd = -1;
    }

    if (sockfd < 0) {
        perror("failed to open and bind socket");
        exit(1);
    }

    rc = listen(sockfd, 1024);
    if (rc < 0) {
        perror("listen failed");
        exit(1);
    }

    freeaddrinfo(info);
    return sockfd;
}

int main(int argc, char **argv)
{
    struct sockaddr_storage cliaddr;
    socklen_t cliaddrlen;
    char cliport[PORTCAP], clihost[HOSTCAP];
    int rc, sockfd, connfd;

    if (argc < 2) {
        fprintf(stderr, "%s [PORT]\n", argv[0]);
        return 1;
    }

    sockfd = open_listensock(argv[1]);
    printf("Server is listening on port %s\n", argv[1]);

    for (;;) {
        cliaddrlen = sizeof(struct sockaddr_storage);
        connfd = accept(sockfd, (struct sockaddr *) &cliaddr, &cliaddrlen);
        if (connfd < 0) {
            perror("accept failed");
            continue;
        }

        rc = getnameinfo((struct sockaddr *) &cliaddr, cliaddrlen,
                         clihost, HOSTCAP, cliport, PORTCAP,
                         NI_NUMERICSERV | NI_NUMERICHOST);
        if (rc != 0) {
            fprintf(stderr, "getnameinfo failed: %s\n", gai_strerror(rc));
        } else {
            printf("Client %s:%s connected\n", clihost, cliport);
        }

        close(connfd);
    }

    close(sockfd);
    return 0;
}
