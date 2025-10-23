#include "../include/sera.h"
#include "../include/config.h"
#include "../include/router.h"
#include "../include/response.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <limits.h>

// Forward declaration for dispatcher
http_response_t *dispatch_request(http_request_t *req);

#define BACKLOG 10

// ───────────────────────────────
// Timestamped logging utility
// ───────────────────────────────
void sera_log(const char *fmt, ...) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm);

    printf("[Sera] %s: ", timestamp);

    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    printf("\n");
}

// ───────────────────────────────
// Simple TCP server setup
// ───────────────────────────────
int start_server(int port) {
    int sockfd;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        exit(1);
    }

    if (listen(sockfd, BACKLOG) < 0) {
        perror("listen");
        close(sockfd);
        exit(1);
    }

    return sockfd;
}

// ───────────────────────────────
// HTTP client handler
// ───────────────────────────────
void handle_client(int client_fd, const char *root, const char *index) {
    char buf[4096];
    int bytes = read(client_fd, buf, sizeof(buf) - 1);
    if (bytes <= 0) return;

    buf[bytes] = '\0';
    char method[8], path[1024];
    sscanf(buf, "%s %s", method, path);

    // Resolve path
    char resolved[2048];
    if (strcmp(path, "/") == 0)
        snprintf(resolved, sizeof(resolved), "%s/%s", root, index);
    else
        snprintf(resolved, sizeof(resolved), "%s%s", root, path);

    sera_log("Request for %s", resolved);

    http_request_t req = { .path = resolved };
    http_response_t *res = dispatch_request(&req);

    if (!res) {
        http_response_t err = {
            .status = 500,
            .content_type = "text/plain",
            .body = "Internal Server Error",
            .body_len = strlen("Internal Server Error")
        };
        send_http_response(client_fd, &err);
        return;
    }

    send_http_response(client_fd, res);
    free_http_response(res);
    close(client_fd);
}

// ───────────────────────────────
// Main entrypoint
// ───────────────────────────────
int main(int argc, char **argv) {
    sera_config_t cfg;
    char cwd[PATH_MAX];
    char confpath[PATH_MAX + 32];

    // Determine launch directory (not binary location)
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd failed");
        exit(1);
    }

    snprintf(confpath, sizeof(confpath), "%s/sera.conf", cwd);
    int loaded = load_config(confpath, &cfg);

    if (!loaded) {
        sera_log("⚠️ No config found in %s — using defaults", confpath);
    } else {
        sera_log("Loaded config file: %s", confpath);
    }

    sera_log("Port=%d Root=%s Index=%s", cfg.port, cfg.root, cfg.index);

    int server_fd = start_server(cfg.port);
    sera_log("Listening on port %d", cfg.port);

    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    while (1) {
        int client_fd = accept(server_fd, (struct sockaddr*)&client, &len);
        if (client_fd < 0) {
            sera_log("Accept failed: %s", strerror(errno));
            continue;
        }

        sera_log("Connection from %s", inet_ntoa(client.sin_addr));
        handle_client(client_fd, cfg.root, cfg.index);
    }

    close(server_fd);
    return 0;
}
