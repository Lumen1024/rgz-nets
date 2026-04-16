#pragma once

#include <sys/types.h>

void notify_client_add(pid_t pid, int pipe_read_fd, int client_fd);
void notify_client_remove(pid_t pid);
void notify_dispatch();
