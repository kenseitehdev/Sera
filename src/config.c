#include "../include/config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Trim spaces from both ends
static void trim(char *s) {
    char *end;
    while (isspace((unsigned char)*s)) s++;
    if (*s == 0) return;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
}

// Default config
static void set_defaults(sera_config_t *cfg) {
    cfg->port = 8080;
    strcpy(cfg->root, "./scripts");
    strcpy(cfg->index, "app.lua");
}

int load_config(const char *path, sera_config_t *cfg) {
    FILE *f = fopen(path, "r");
    set_defaults(cfg);

    if (!f) return 0; // not found, use defaults

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '#' || strlen(line) == 0) continue;

        char *eq = strchr(line, '=');
        if (!eq) continue;

        *eq = '\0';
        char key[64], val[256];
        strcpy(key, line);
        strcpy(val, eq + 1);
        trim(key);
        trim(val);

        if (strcmp(key, "port") == 0) {
            cfg->port = atoi(val);
        } else if (strcmp(key, "root") == 0) {
            strncpy(cfg->root, val, sizeof(cfg->root) - 1);
        } else if (strcmp(key, "index") == 0) {
            strncpy(cfg->index, val, sizeof(cfg->index) - 1);
        }
    }

    fclose(f);
    return 1;
}
