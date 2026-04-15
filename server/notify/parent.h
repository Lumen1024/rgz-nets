#pragma once

#include <sys/types.h>

void notify_parent_register(pid_t pid, int pipe_read_fd, int client_fd);
void notify_parent_unregister(pid_t pid);
void notify_dispatch();
