#pragma once

#include "protocol.h"

int send_request(int socket_fd, Request request);
int parse_request(char *data, Request *request);
void free_request(Request *request);
