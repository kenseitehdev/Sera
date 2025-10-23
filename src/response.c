
#include "../include/response.h"
#include <unistd.h>

void send_http_response(int client_fd, http_response_t *res) {
    char header[1024];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n\r\n",
             res->status, res->content_type, res->body_len);

    write(client_fd, header, strlen(header));
    write(client_fd, res->body, res->body_len);
}

void free_http_response(http_response_t *res) {
    if (!res) return;
    if (res->body) free(res->body);
    free(res);
}
