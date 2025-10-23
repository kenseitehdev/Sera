
#include "../include/router.h"

handler_entry_t handlers[] = {
    { ".lua", handle_lua },
    { ".py", handle_python },
    { ".php", handle_php },
    { ".js", handle_js },
    { ".db", handle_sqlite },
    { ".cmod", handle_cmod },
    { NULL, NULL }
};

handler_func_t find_handler(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return NULL;
    for (int i = 0; handlers[i].ext; i++) {
        if (strcmp(dot, handlers[i].ext) == 0)
            return handlers[i].func;
    }
    return NULL;
}
