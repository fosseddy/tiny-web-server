#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    char *query, *p, body[256];
    int a, b, sum;

    query = getenv("QUERY_STRING");
    if (query == NULL) {
        sprintf(body, "<html>"
                      "<head><title>Adder Error</title></head>"
                      "<body>"
                      "<p>400 Bad Request</p>"
                      "<p>No arguments provided to adder</p>"
                      "</body>"
                      "</html>\r\n");
        printf("HTTP/1.1 400 Bad Request\r\n"
               "Content-type: text/html\r\n"
               "Content-length: %d\r\n\r\n", strlen(body));
        printf("%s", body);
        return 1;
    }

    p = strchr(query, '&');
    if (p == NULL) {
        sprintf(body, "<html>"
                      "<head><title>Adder Error</title></head>"
                      "<body>"
                      "<p>400 Bad Request</p>"
                      "<p>Adder requires 2 arguments</p>"
                      "</body>"
                      "</html>\r\n");
        printf("HTTP/1.1 400 Bad Request\r\n"
               "Content-type: text/html\r\n"
               "Content-length: %d\r\n\r\n", strlen(body));
        printf("%s", body);
        return 1;
    }

    *p++ = '\0';
    a = atoi(query);
    b = atoi(p);
    sum = a + b;

    sprintf(body, "<html>"
            "<head><title>Adder</title></head>"
            "<body><h1>%d + %d = %d</h1></body>"
            "</html>\r\n", a, b, sum);
    printf("HTTP/1.1 400 Bad Request\r\n"
            "Content-type: text/html\r\n"
            "Content-length: %d\r\n\r\n", strlen(body));
    printf("%s", body);
    return 0;
}
