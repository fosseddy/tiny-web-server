#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <assert.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#define HOSTCAP 256
#define PORTCAP 8

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

ssize_t net_readline(int sockfd, char *buf, size_t bufsize)
{
    char ch;
    size_t i;
    ssize_t n;

    for (i = 0; i < bufsize - 1; ++i) {
        n = read(sockfd, &ch, 1);
        if (n < 0) return -1;
        if (n == 0) break;
        if (ch == '\n') {
            // remove \r
            assert(i > 0);
            buf--;
            i--;
            break;
        }
        *buf++ = ch;
    }

    *buf = '\0';
    return i;
}

void read_reqline(int sockfd, char *method, char *uri, char *http_ver)
{
    char buf[1024];
    ssize_t n;
    int rc;

    n = net_readline(sockfd, buf, sizeof(buf));
    assert(n >= 0);
    rc = sscanf(buf, "%s %s %s", method, uri, http_ver);
    assert(rc == 3);
    printf("%s\n", buf);
}

void read_headers(int sockfd)
{
    char buf[1024];
    ssize_t n;

    for (;;) {
        n = net_readline(sockfd, buf, sizeof(buf));
        assert(n >= 0);
        if (n == 0) break;
        printf("%s\n", buf);
    }
}

int parse_uri(char *uri, char *filepath, char *cgiargs)
{
    int rc;
    char *p;

    if (strstr(uri, "/static/") != NULL) {
        rc = 1;
        strcpy(filepath, ".");
        strcat(filepath, uri);
        if (filepath[strlen(filepath) - 1] == '/') {
            strcat(filepath, "index.html");
        }
    } else if (strstr(uri, "/cgi/") != NULL) {
        rc = 0;
        strcpy(cgiargs, "");
        p = strchr(uri, '?');
        if (p != NULL) {
            *p = '\0';
            strcpy(cgiargs, p + 1);
        }
        strcpy(filepath, ".");
        strcat(filepath, uri);
    } else {
        rc = -1;
    }

    return rc;
}

void write_error(int sockfd, char *status_code, char *status_msg, char *msg)
{
    char buf[512], body[1024];
    size_t buflen, bodylen;
    ssize_t n;

    sprintf(body, "<html>"
                  "<head><title>Tiny Error</title></head>"
                  "<body>"
                  "<p>%s %s</p>"
                  "<p>%s</p>"
                  "<hr />"
                  "<p>The Tiny Web Server</p>"
                  "</body>"
                  "</html>\r\n", status_code, status_msg, msg);
    bodylen = strlen(body);

    sprintf(buf, "HTTP/1.1 %s %s\r\n"
                 "Content-type: text/html\r\n"
                 "Content-length: %d\r\n\r\n", status_code, status_msg,
                                               bodylen);
    buflen = strlen(buf);

    n = write(sockfd, buf, buflen);
    assert(n == buflen);
    n = write(sockfd, body, bodylen);
    assert(n == bodylen);
}

void get_filetype(char *path, char *type)
{
    if (strstr(path, ".html") != NULL) {
        strcpy(type, "text/html");
    } else if (strstr(path, ".gif") != NULL) {
        strcpy(type, "image/gif");
    } else if (strstr(path, ".png") != NULL) {
        strcpy(type, "image/png");
    } else if (strstr(path, ".jpg") != NULL) {
        strcpy(type, "image/jpeg");
    } else {
        strcpy(type, "text/plain");
    }
}

void serve_static(int sockfd, char *filepath, off_t filesize)
{
    char filetype[128], buf[256], *filedata;
    int fd, rc;
    size_t buflen;
    ssize_t n;

    get_filetype(filepath, filetype);

    sprintf(buf, "HTTP/1.1 200 OK\r\n"
                 "Content-type: %s\r\n"
                 "Content-length: %d\r\n\r\n", filetype, filesize);
    buflen = strlen(buf);

    fd = open(filepath, O_RDONLY);
    assert(fd > 0);
    filedata = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fd, 0);
    assert(filedata != MAP_FAILED);

    n = write(sockfd, buf, buflen);
    assert(n == buflen);
    n = write(sockfd, filedata, filesize);
    assert(n == filesize);

    close(fd);
    rc = munmap(filedata, filesize);
    assert(rc == 0);
}

void serve_cgi()
{
}

void handle_req(int fd)
{
    char method[8], uri[512], http_ver[16], filepath[256], cgiargs[256];
    int resource_type, rc;
    struct stat statbuf;

    read_reqline(fd, method, uri, http_ver);

    if (strcmp(method, "GET") != 0) {
        write_error(fd, "501", "Not implemented", "Only GET is supported");
        return;
    }

    read_headers(fd);

    resource_type = parse_uri(uri, filepath, cgiargs);
    if (resource_type < 0) {
        write_error(fd, "404", "Not found", "Unknown resource type");
        return;
    }

    rc = stat(filepath, &statbuf);
    if (rc < 0) {
        write_error(fd, "404", "Not found", "File does not exist");
        return;
    }

    if (resource_type == 1) {
        if (!S_ISREG(statbuf.st_mode) || !(statbuf.st_mode & S_IRUSR)) {
            write_error(fd, "403", "Forbidden",
                        "Not allowed to read this file");
            return;
        }
        serve_static(fd, filepath, statbuf.st_size);
        return;
    }

    if (!S_ISREG(statbuf.st_mode) || !(statbuf.st_mode & S_IXUSR)) {
        write_error(fd, "403", "Forbidden",
                    "Not allowed to execute this file");
        return;
    }

    serve_cgi();
}

// @Todo(art): implement net_read, net_write
// @Todo(art): better CRLF handle
int main(int argc, char **argv)
{
    struct sockaddr_storage cliaddr;
    socklen_t cliaddrlen;
    char cliport[PORTCAP], clihost[HOSTCAP];
    int rc, sockfd, clifd;

    if (argc < 2) {
        fprintf(stderr, "%s [PORT]\n", argv[0]);
        return 1;
    }

    sockfd = open_listensock(argv[1]);
    printf("Server is listening on port %s\n", argv[1]);

    for (;;) {
        cliaddrlen = sizeof(struct sockaddr_storage);
        clifd = accept(sockfd, (struct sockaddr *) &cliaddr, &cliaddrlen);
        if (clifd < 0) {
            perror("accept failed");
            return 1;
        }

        rc = getnameinfo((struct sockaddr *) &cliaddr, cliaddrlen,
                         clihost, HOSTCAP, cliport, PORTCAP,
                         NI_NUMERICSERV | NI_NUMERICHOST);
        if (rc != 0) {
            fprintf(stderr, "getnameinfo failed: %s\n", gai_strerror(rc));
            return 1;
        }

        printf("-- Client %s:%s connected --\n", clihost, cliport);

        handle_req(clifd);
        close(clifd);

        printf("-- Client %s:%s disconnected --\n", clihost, cliport);
    }

    close(sockfd);
    return 0;
}
