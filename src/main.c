/*
 * Sera - Polyglot Web Server
 * Multi-language application server with Python, Lua, PHP, Node.js, Ruby
 * Dynamic routing, SQLite3 integration, and static file serving
 * 
 * Compile: make
 * Run: bin/sera server.conf
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>

// Lua
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// Python
#include <Python.h>

// SQLite
#include <sqlite3.h>

#define MAX_ROUTES 100
#define MAX_PATH 512
#define BUFFER_SIZE 16384
#define MAX_HEADERS 50

typedef enum { 
    ROUTE_STATIC, 
    ROUTE_LUA, 
    ROUTE_PYTHON, 
    ROUTE_PHP, 
    ROUTE_NODE, 
    ROUTE_RUBY,
    ROUTE_FORWARD 
} route_type_t;

typedef struct {
    char pattern[MAX_PATH];
    char target[MAX_PATH];
    route_type_t type;
    char methods[64];
} route_t;

typedef struct {
    int port;
    char root_dir[MAX_PATH];
    char script_dir[MAX_PATH];
    char db_path[MAX_PATH];
    int worker_threads;
    route_t routes[MAX_ROUTES];
    int route_count;
    
    // Runtime paths
    char php_cgi[MAX_PATH];
    char node_bin[MAX_PATH];
    char ruby_bin[MAX_PATH];
} config_t;

typedef struct {
    char key[64];
    char value[256];
} route_param_t;

typedef struct {
    char method[16];
    char path[MAX_PATH];
    char query[MAX_PATH];
    char headers[MAX_HEADERS][256];
    int header_count;
    char body[BUFFER_SIZE];
    int body_len;
    route_param_t params[10];
    int param_count;
} http_request_t;

config_t config;
sqlite3 *db = NULL;
int python_initialized = 0;

// Parse configuration
int parse_config(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("Config file not found");
        return -1;
    }
    
    char line[512];
    config.port = 8080;
    config.worker_threads = 4;
    strcpy(config.root_dir, "./public");
    strcpy(config.script_dir, "./scripts");
    strcpy(config.db_path, "./data.db");
    strcpy(config.php_cgi, "/usr/bin/php-cgi");
    strcpy(config.node_bin, "/usr/bin/node");
    strcpy(config.ruby_bin, "/usr/bin/ruby");
    config.route_count = 0;
    
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        
        if (sscanf(line, "port = %d", &config.port) == 1) continue;
        if (sscanf(line, "threads = %d", &config.worker_threads) == 1) continue;
        if (sscanf(line, "root = %s", config.root_dir) == 1) continue;
        if (sscanf(line, "script_dir = %s", config.script_dir) == 1) continue;
        if (sscanf(line, "database = %s", config.db_path) == 1) continue;
        if (sscanf(line, "php_cgi = %s", config.php_cgi) == 1) continue;
        if (sscanf(line, "node_bin = %s", config.node_bin) == 1) continue;
        if (sscanf(line, "ruby_bin = %s", config.ruby_bin) == 1) continue;
        
        char method[16], pattern[MAX_PATH], type[16], target[MAX_PATH];
        if (sscanf(line, "route %s %s %s %s", method, pattern, type, target) == 4) {
            route_t *r = &config.routes[config.route_count++];
            strcpy(r->methods, method);
            strcpy(r->pattern, pattern);
            strcpy(r->target, target);
            
            if (strcmp(type, "lua") == 0) r->type = ROUTE_LUA;
            else if (strcmp(type, "python") == 0) r->type = ROUTE_PYTHON;
            else if (strcmp(type, "php") == 0) r->type = ROUTE_PHP;
            else if (strcmp(type, "node") == 0) r->type = ROUTE_NODE;
            else if (strcmp(type, "ruby") == 0) r->type = ROUTE_RUBY;
            else if (strcmp(type, "forward") == 0) r->type = ROUTE_FORWARD;
            else r->type = ROUTE_STATIC;
        }
    }
    
    fclose(f);
    return 0;
}

// Enhanced pattern matching with parameter extraction
int match_route_with_params(const char *pattern, const char *path, route_param_t *params, int *param_count) {
    const char *p = pattern;
    const char *s = path;
    *param_count = 0;
    
    while (*p && *s) {
        if (*p == ':') {
            // Named parameter
            p++;
            char param_name[64] = {0};
            int name_len = 0;
            int optional = 0;
            
            // Extract parameter name
            while (*p && *p != '/' && *p != '?') {
                if (*p == '?') {
                    optional = 1;
                    p++;
                    break;
                }
                param_name[name_len++] = *p++;
            }
            
            // Extract parameter value
            char param_value[256] = {0};
            int value_len = 0;
            while (*s && *s != '/') {
                param_value[value_len++] = *s++;
            }
            
            if (value_len == 0 && !optional) {
                return 0;
            }
            
            if (value_len > 0) {
                strcpy(params[*param_count].key, param_name);
                strcpy(params[*param_count].value, param_value);
                (*param_count)++;
            }
            
        } else if (*p == '*') {
            p++;
            if (*p == '\0') return 1;
            while (*s && *s != *p) s++;
            
        } else if (*p == *s) {
            p++;
            s++;
        } else {
            return 0;
        }
    }
    
    while (*p == '/' || (*p == ':' && strchr(p, '?'))) {
        if (*p == '/') p++;
        if (*p == ':') {
            while (*p && *p != '/') p++;
        }
    }
    
    return (*p == '\0' && *s == '\0') || (*p == '*' && *(p+1) == '\0');
}

int match_route(const char *pattern, const char *path) {
    route_param_t dummy[10];
    int count;
    return match_route_with_params(pattern, path, dummy, &count);
}

route_t* find_route(const char *method, const char *path, route_param_t *params, int *param_count) {
    for (int i = 0; i < config.route_count; i++) {
        route_t *r = &config.routes[i];
        if ((strcmp(r->methods, "*") == 0 || strcmp(r->methods, method) == 0) &&
            match_route_with_params(r->pattern, path, params, param_count)) {
            return r;
        }
    }
    *param_count = 0;
    return NULL;
}

void parse_http_request(const char *req, http_request_t *parsed) {
    sscanf(req, "%s %s", parsed->method, parsed->path);
    
    char *q = strchr(parsed->path, '?');
    if (q) {
        strcpy(parsed->query, q + 1);
        *q = '\0';
    } else {
        parsed->query[0] = '\0';
    }
    
    parsed->header_count = 0;
    const char *line = req;
    while ((line = strchr(line, '\n')) != NULL) {
        line++;
        if (*line == '\r' || *line == '\n') {
            line++;
            break;
        }
        if (parsed->header_count < MAX_HEADERS) {
            sscanf(line, "%255[^\r\n]", parsed->headers[parsed->header_count++]);
        }
    }
    
    if (line) {
        strncpy(parsed->body, line, BUFFER_SIZE - 1);
        parsed->body_len = strlen(parsed->body);
    }
}

char* execute_lua(const char *script_path, http_request_t *req) {
    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    
    lua_newtable(L);
    lua_pushstring(L, req->method); lua_setfield(L, -2, "method");
    lua_pushstring(L, req->path); lua_setfield(L, -2, "path");
    lua_pushstring(L, req->query); lua_setfield(L, -2, "query");
    lua_pushstring(L, req->body); lua_setfield(L, -2, "body");
    
    lua_newtable(L);
    for (int i = 0; i < req->param_count; i++) {
        lua_pushstring(L, req->params[i].value);
        lua_setfield(L, -2, req->params[i].key);
    }
    lua_setfield(L, -2, "params");
    
    lua_setglobal(L, "request");
    
    lua_pushlightuserdata(L, db);
    lua_setglobal(L, "db_handle");
    
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", config.script_dir, script_path);
    
    if (luaL_dofile(L, full_path) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        char *result = malloc(512);
        snprintf(result, 512, "HTTP/1.1 500 Internal Server Error\r\n\r\nLua Error: %s", err);
        lua_close(L);
        return result;
    }
    
    lua_getglobal(L, "response");
    const char *resp = lua_tostring(L, -1);
    char *result = strdup(resp ? resp : "HTTP/1.1 200 OK\r\n\r\nNo response");
    lua_close(L);
    return result;
}

char* execute_python(const char *script_path, http_request_t *req) {
    if (!python_initialized) {
        Py_Initialize();
        python_initialized = 1;
    }
    
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", config.script_dir, script_path);
    
    FILE *fp = fopen(full_path, "r");
    if (!fp) {
        char *result = malloc(256);
        snprintf(result, 256, "HTTP/1.1 404 Not Found\r\n\r\nScript not found");
        return result;
    }
    
    PyObject *pModule = PyImport_AddModule("__main__");
    PyObject *pDict = PyModule_GetDict(pModule);
    
    PyObject *environ = PyDict_New();
    PyDict_SetItemString(environ, "REQUEST_METHOD", PyUnicode_FromString(req->method));
    PyDict_SetItemString(environ, "PATH_INFO", PyUnicode_FromString(req->path));
    PyDict_SetItemString(environ, "QUERY_STRING", PyUnicode_FromString(req->query));
    PyDict_SetItemString(environ, "CONTENT_LENGTH", PyLong_FromLong(req->body_len));
    PyDict_SetItemString(environ, "wsgi.input", PyBytes_FromStringAndSize(req->body, req->body_len));
    
    PyObject *params = PyDict_New();
    for (int i = 0; i < req->param_count; i++) {
        PyDict_SetItemString(params, req->params[i].key, 
                           PyUnicode_FromString(req->params[i].value));
    }
    PyDict_SetItemString(environ, "route.params", params);
    
    PyDict_SetItemString(pDict, "environ", environ);
    
    PyRun_SimpleFile(fp, full_path);
    fclose(fp);
    
    PyObject *pResp = PyDict_GetItemString(pDict, "response");
    char *result;
    if (pResp && PyUnicode_Check(pResp)) {
        const char *resp_str = PyUnicode_AsUTF8(pResp);
        result = strdup(resp_str);
    } else {
        result = strdup("HTTP/1.1 200 OK\r\n\r\nNo response");
    }
    
    Py_DECREF(environ);
    return result;
}

char* execute_cgi(const char *interpreter, const char *script_path, http_request_t *req) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s/%s", config.script_dir, script_path);
    
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return strdup("HTTP/1.1 500 Internal Server Error\r\n\r\nPipe error");
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        
        setenv("REQUEST_METHOD", req->method, 1);
        setenv("PATH_INFO", req->path, 1);
        setenv("QUERY_STRING", req->query, 1);
        setenv("CONTENT_LENGTH", "0", 1);
        setenv("SCRIPT_FILENAME", full_path, 1);
        
        for (int i = 0; i < req->param_count; i++) {
            char env_name[128];
            snprintf(env_name, sizeof(env_name), "ROUTE_PARAM_%s", req->params[i].key);
            for (char *p = env_name; *p; p++) *p = toupper(*p);
            setenv(env_name, req->params[i].value, 1);
        }
        
        execl(interpreter, interpreter, full_path, NULL);
        exit(1);
    }
    
    close(pipefd[1]);
    
    char *output = malloc(BUFFER_SIZE);
    int total = 0;
    int n;
    while ((n = read(pipefd[0], output + total, BUFFER_SIZE - total - 1)) > 0) {
        total += n;
    }
    output[total] = '\0';
    close(pipefd[0]);
    
    waitpid(pid, NULL, 0);
    
    if (strncmp(output, "HTTP/", 5) != 0) {
        char *with_headers = malloc(BUFFER_SIZE);
        snprintf(with_headers, BUFFER_SIZE, 
                "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n%s", output);
        free(output);
        return with_headers;
    }
    
    return output;
}

char* serve_static_file(const char *path) {
    char full_path[MAX_PATH];
    snprintf(full_path, sizeof(full_path), "%s%s", config.root_dir, path);
    
    struct stat st;
    if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        strcat(full_path, "/index.html");
    }
    
    FILE *f = fopen(full_path, "rb");
    if (!f) {
        char *resp = malloc(256);
        snprintf(resp, 256, "HTTP/1.1 404 Not Found\r\n\r\n404 - File Not Found");
        return resp;
    }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = malloc(fsize + 512);
    
    const char *ext = strrchr(full_path, '.');
    const char *ctype = "text/plain";
    if (ext) {
        if (strcmp(ext, ".html") == 0) ctype = "text/html";
        else if (strcmp(ext, ".js") == 0) ctype = "application/javascript";
        else if (strcmp(ext, ".css") == 0) ctype = "text/css";
        else if (strcmp(ext, ".json") == 0) ctype = "application/json";
        else if (strcmp(ext, ".png") == 0) ctype = "image/png";
        else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) ctype = "image/jpeg";
    }
    
    sprintf(content, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %ld\r\n\r\n", ctype, fsize);
    int hlen = strlen(content);
    fread(content + hlen, 1, fsize, f);
    fclose(f);
    
    return content;
}

void* handle_client(void *arg) {
    int client_sock = *(int*)arg;
    free(arg);
    
    char buffer[BUFFER_SIZE];
    int bytes = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes <= 0) {
        close(client_sock);
        return NULL;
    }
    buffer[bytes] = '\0';
    
    http_request_t req = {0};
    parse_http_request(buffer, &req);
    
    printf("[%s] %s\n", req.method, req.path);
    
    char *response = NULL;
    route_t *route = find_route(req.method, req.path, req.params, &req.param_count);
    
    if (route) {
        switch (route->type) {
            case ROUTE_LUA:
                response = execute_lua(route->target, &req);
                break;
            case ROUTE_PYTHON:
                response = execute_python(route->target, &req);
                break;
            case ROUTE_PHP:
                response = execute_cgi(config.php_cgi, route->target, &req);
                break;
            case ROUTE_NODE:
                response = execute_cgi(config.node_bin, route->target, &req);
                break;
            case ROUTE_RUBY:
                response = execute_cgi(config.ruby_bin, route->target, &req);
                break;
            case ROUTE_STATIC:
                response = serve_static_file(route->target);
                break;
            default:
                response = strdup("HTTP/1.1 501 Not Implemented\r\n\r\nRoute type not implemented");
        }
    } else {
        response = serve_static_file(req.path);
    }
    
    send(client_sock, response, strlen(response), 0);
    free(response);
    close(client_sock);
    
    return NULL;
}

int main(int argc, char *argv[]) {
    const char *config_file = argc > 1 ? argv[1] : "server.conf";
    
    if (parse_config(config_file) != 0) {
        fprintf(stderr, "Failed to parse config\n");
        return 1;
    }
    
    if (sqlite3_open(config.db_path, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port = htons(config.port)
    };
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        return 1;
    }
    
    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        return 1;
    }
    
    printf("Sera - Polyglot Application Server\n");
    printf("===================================\n");
    printf("Listening on port %d\n", config.port);
    printf("Supported: Lua, Python, PHP, Node.js, Ruby, Static files\n");
    printf("Routes: %d configured\n", config.route_count);
    printf("Worker threads: %d\n", config.worker_threads);
    printf("\nPress Ctrl+C to stop\n\n");
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (*client_sock < 0) {
            free(client_sock);
            continue;
        }
        
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_sock);
        pthread_detach(tid);
    }
    
    if (python_initialized) Py_Finalize();
    sqlite3_close(db);
    close(server_sock);
    return 0;
}
