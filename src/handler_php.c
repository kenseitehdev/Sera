#include "../include/sera.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

http_response_t *handle_php(http_request_t *req) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "php %s 2>&1", req->path + 1);

    FILE *fp = popen(cmd, "r");
    if (!fp) {
        http_response_t *res = malloc(sizeof(http_response_t));
        res->status = 500;
        res->content_type = "text/plain";
        res->body = strdup("Failed to execute PHP script");
        res->body_len = strlen(res->body);
        return res;
    }

    char buffer[8192];
    size_t total = 0;
    char *output = NULL;

    while (fgets(buffer, sizeof(buffer), fp)) {
        size_t chunk = strlen(buffer);
        output = realloc(output, total + chunk + 1);
        memcpy(output + total, buffer, chunk);
        total += chunk;
        output[total] = '\0';
    }
    pclose(fp);

    http_response_t *res = malloc(sizeof(http_response_t));
    res->status = 200;
    res->content_type = "text/html";
    res->body = output ? output : strdup("");
    res->body_len = strlen(res->body);
    return res;
}
