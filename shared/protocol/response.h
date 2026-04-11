#pragma once

#include <protocol.h>

int send_response(int socket_fd, Response response);
int parse_response(char *data, Response *response);
void free_response(Response *response);
Response make_error(ErrorCode code);
Response make_success(cJSON *content);
