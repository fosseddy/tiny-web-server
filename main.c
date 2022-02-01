#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

int main(void)
{
    char *host = "localhost";
    char *port = "3000";

    struct addrinfo *res, hints;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE | AI_NUMERICSERV | AI_ADDRCONFIG;

    int err = getaddrinfo(host, port, &hints, &res);
    if (err > 0) {
        fprintf(stderr, "%s\n", gai_strerror(err));
        exit(1);
    }

    int sockfd = -1;
    int optval = 1;
    for (struct addrinfo *i = res; res != NULL; i = i->ai_next) {
        sockfd = socket(i->ai_family, i->ai_socktype, i->ai_protocol);
        if (sockfd < 0) continue;

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));

        if (bind(sockfd, i->ai_addr, i->ai_addrlen) == 0) break;

        close(sockfd);
        sockfd = -1;
    }

    freeaddrinfo(res);

    if (sockfd < 0) {
        fprintf(stderr, "Could not create socket\n");
        exit(1);
    }

    if (listen(sockfd, 1024) < 0) {
        fprintf(stderr, "%s\n", strerror(errno));
        exit(1);
    }

    struct sockaddr_storage cliaddr;
    while (1) {
        socklen_t cliaddrlen = sizeof(struct sockaddr_storage);

        int connfd = accept(sockfd, (struct sockaddr *) &cliaddr, &cliaddrlen);
        if (connfd < 0) {
            fprintf(stderr, "%s\n", strerror(errno));
            continue;
        }

        char buf[1000] = {0};
        read(connfd, buf, 1000);
        printf("%s\n", buf);

        char method[10] = {0};
        char uri[1000] = {0};
        char http_ver[10] = {0};
        sscanf(buf, "%s %s %s", method, uri, http_ver);

        if (strcmp(method, "GET") == 0 && strcmp(uri, "/") == 0) {
            char content[1000] = {0};
            char contentlen[1000] = {0};

            sprintf(content, "<h1>Method</h1>\r\n"
                             "<p>%s</p>\r\n"
                             "<h1>URI</h1>\r\n"
                             "<p>%s</p>\r\n"
                             "<h1>Http Version</h1>\r\n"
                             "<p>%s</p>\r\n", method, uri, http_ver);

            sprintf(contentlen, "Content-Length: %lu\r\n", strlen(content));

            write(connfd, "HTTP/1.1 200 OK\r\n", 17);
            write(connfd, "Server: tiny-web-server\r\n", 25);
            write(connfd, "Content-Type: text/html\r\n", 25);
            write(connfd, contentlen, strlen(contentlen));
            write(connfd, "\r\n", 2);
            write(connfd, content, strlen(content));
        } else {
            write(connfd, "HTTP/1.1 404 Not Found\r\n", 17);
            write(connfd, "Server: tiny-web-server\r\n", 25);
            write(connfd, "Content-Type: text/html\r\n", 25);
            write(connfd, "Content-Length: 20\r\n", 20);
            write(connfd, "\r\n", 2);
            write(connfd, "<h1>Not Found</h1>\r\n", 20);
        }

        close(connfd);
    }

    close(sockfd);

    return 0;
}
