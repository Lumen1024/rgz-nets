#pragma once

#include <protocol.h>

void notify_child_init(int pipe_write_fd);

void notify_register(const char *login);
void notify_unregister(int socket_fd);

void notify_user(const char *login, Notification notif);
void notify_chat(const char *chat_name, Notification notif);
