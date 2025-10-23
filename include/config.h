
#ifndef CONFIG_H
#define CONFIG_H

typedef struct {
    int port;
    char root[256];
    char index[128];
} sera_config_t;

int load_config(const char *filename, sera_config_t *cfg);

#endif
