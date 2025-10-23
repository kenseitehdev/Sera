
#include "../include/sera.h"
#include <dlfcn.h>

typedef const char* (*cmod_main_t)(void);

http_response_t *handle_cmod(http_request_t *req) {
    void *handle = dlopen(req->path + 1, RTLD_NOW);
    if (!handle) {
        http_response_t *res = malloc(sizeof(http_response_t));
        res->status = 500;
        res->content_type = "text/plain";
        res->body = strdup(dlerror());
        res->body_len = strlen(res->body);
        return res;
    }
    cmod_main_t main_func = (cmod_main_t)dlsym(handle, "cmod_main");
    const char *output = main_func ? main_func() : "no cmod_main found";
    http_response_t *res = malloc(sizeof(http_response_t));
    res->status = 200;
    res->content_type = "text/plain";
    res->body = strdup(output);
    res->body_len = strlen(res->body);
    dlclose(handle);
    return res;
}
