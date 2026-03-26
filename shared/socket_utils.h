#pragma once

#include <stddef.h>

int read_message(int socket_fd, char *buffer, size_t max_len);
int write_message(int socket_fd, const char *data);
int get_peer_ip(int socket_fd, char *ip_out);
