
#include "../include/sera.h"
#include <quickjs/quickjs.h>

http_response_t *handle_js(http_request_t *req) {
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    FILE *f = fopen(req->path + 1, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);
    char *code = malloc(len+1);
    fread(code, 1, len, f);
    code[len] = '\0';
    fclose(f);

    JSValue result = JS_Eval(ctx, code, len, req->path, JS_EVAL_TYPE_GLOBAL);
    const char *str = JS_ToCString(ctx, result);
    http_response_t *res = malloc(sizeof(http_response_t));
    res->status = 200;
    res->content_type = "text/html";
    res->body = strdup(str);
    res->body_len = strlen(str);
    JS_FreeCString(ctx, str);
    JS_FreeValue(ctx, result);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    free(code);
    return res;
}
