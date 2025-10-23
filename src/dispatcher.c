
#include "../include/sera.h"
#include "../include/router.h"
#include "../include/response.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int match_dynamic(const char *pattern, const char *path, char *param_name, char *param_value) {
    const char *open = strchr(pattern, '<');
    const char *close = strchr(pattern, '>');
    if (!open || !close || close < open) return strcmp(pattern, path) == 0;

    char prefix[128], suffix[128];
    strncpy(prefix, pattern, open - pattern);
    prefix[open - pattern] = '\0';
    strcpy(suffix, close + 1);

    if (strncmp(path, prefix, strlen(prefix)) != 0) return 0;
    const char *param_start = path + strlen(prefix);
    const char *param_end = strstr(param_start, suffix);
    if (!param_end) return 0;

    size_t len = close - open - 1;
    strncpy(param_name, open + 1, len);
    param_name[len] = '\0';

    strncpy(param_value, param_start, param_end - param_start);
    param_value[param_end - param_start] = '\0';
    return 1;
}

http_response_t *dispatch_request(http_request_t *req) {
    handler_func_t handler = find_handler(req->path);
    if (!handler) {
        http_response_t *res = malloc(sizeof(http_response_t));
        res->status = 404;
        res->content_type = "text/plain";
        res->body = strdup("Not Found");
        res->body_len = strlen(res->body);
        return res;
    }

    http_response_t *res = handler(req);
    if (!res) return NULL;

    if (strncmp(res->body, "__FORWARD__:", 12) == 0) {
        const char *new_path = res->body + 12;
        free_http_response(res);

        http_request_t fwd_req = { .path = new_path };
        return dispatch_request(&fwd_req);
    }

    return res;
}
