#include "../include/sera.h"
#include <lua5.4/lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <string.h>

static lua_State *L = NULL;

static int l_route(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_getglobal(L, "routes");
    if (lua_isnil(L, -1)) {
        lua_newtable(L);
        lua_setglobal(L, "routes");
        lua_getglobal(L, "routes");
    }

    lua_pushstring(L, path);
    lua_pushvalue(L, 2);
    lua_settable(L, -3);

    return 0;
}

static int l_forward(lua_State *L) {
    const char *path = luaL_checkstring(L, 1);
    lua_pushfstring(L, "__FORWARD__:%s", path);
    return 1;
}

http_response_t *handle_lua(http_request_t *req) {
    if (!L) {
        L = luaL_newstate();
        luaL_openlibs(L);
        lua_register(L, "route", l_route);
        lua_register(L, "forward", l_forward);
        lua_newtable(L);
        lua_setglobal(L, "routes");
    }

    if (luaL_dofile(L, "app.lua") != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        http_response_t *res = malloc(sizeof(http_response_t));
        res->status = 500;
        res->content_type = "text/plain";
        res->body = strdup(err);
        res->body_len = strlen(err);
        return res;
    }

    lua_getglobal(L, "routes");
    lua_pushnil(L);

    while (lua_next(L, -2) != 0) {
        const char *pattern = lua_tostring(L, -2);
        if (strcmp(req->path, pattern) == 0) {
            lua_pushstring(L, req->path);
            if (lua_pcall(L, 1, 1, 0) == LUA_OK) {
                const char *out = lua_tostring(L, -1);
                http_response_t *res = malloc(sizeof(http_response_t));
                res->status = 200;
                res->content_type = "text/plain";
                res->body = strdup(out);
                res->body_len = strlen(out);
                return res;
            }
        }
        lua_pop(L, 1);
    }

    http_response_t *res = malloc(sizeof(http_response_t));
    res->status = 404;
    res->content_type = "text/plain";
    res->body = strdup("Lua route not found");
    res->body_len = strlen(res->body);
    return res;
}
