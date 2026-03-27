#pragma once

#include <sys/types.h>
#include <protocol.h>

// Must be called once in main() before any fork()
void notify_init(void);

// Called in parent after fork() to register child's pipe write-end and client fd
void notify_parent_register(pid_t pid, int child_pipe_read, int client_fd);
void notify_parent_unregister(pid_t pid);

// Called in parent's main loop to forward pending notifications
void notify_dispatch(void);

// Called in child process to set its write-end of the notification pipe
void notify_child_init(int pipe_write_fd);

void notify_chat(const char *chat_name, Notification notif);
void notify_user(const char *login, Notification notif);

void notify_register(int socket_fd, const char *login);
void notify_unregister(int socket_fd);
