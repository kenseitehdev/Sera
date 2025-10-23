#ifndef RESPONSE_H
#define RESPONSE_H
#include "sera.h"

void send_http_response(int client_fd, http_response_t *res);
void free_http_response(http_response_t *res);

#endif
