#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    char *query, *p, *arg1, *arg2, body[256];
    int a = 0, b = 0;

    query = getenv("QUERY_STRING");
    if (query != NULL) {
        p = strchr(query, '&');
        if (p != NULL) {
            *p++ = '\0';

            arg1 = strchr(query, '=');
            if (arg1 != NULL) {
                a = atoi(arg1 + 1);
            }

            arg2 = strchr(p, '=');
            if (arg2 != NULL) {
                b = atoi(arg2 + 1);
            }
        }
    }

    sprintf(body, "<html>"
                  "<head><title>Adder</title></head>"
                  "<body><h1>%d + %d = %d</h1></body>"
                  "</html>\r\n", a, b, a + b);
    printf("HTTP/1.1 200 OK\r\n"
           "Content-type: text/html\r\n"
           "Content-length: %ld\r\n\r\n", strlen(body));
    printf("%s", body);
    return 0;
}
