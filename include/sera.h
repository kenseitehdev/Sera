#ifndef SERA_H
#define SERA_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int status;
    char *content_type;
    char *body;
    size_t body_len;
} http_response_t;

typedef struct {
    const char *method;
    const char *path;
    const char *query;
    const char *body;
    size_t body_len;
} http_request_t;

typedef http_response_t *(*handler_func_t)(http_request_t *req);

typedef struct {
    const char *ext;
    handler_func_t func;
} handler_entry_t;

http_response_t *handle_lua(http_request_t *req);
http_response_t *handle_python(http_request_t *req);
http_response_t *handle_php(http_request_t *req);
http_response_t *handle_js(http_request_t *req);
http_response_t *handle_sqlite(http_request_t *req);
http_response_t *handle_cmod(http_request_t *req);

#endif
