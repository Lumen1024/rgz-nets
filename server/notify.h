#pragma once

#include <protocol.h>

void notify_chat(const char *chat_name, Notification notif);
void notify_user(const char *login, Notification notif);

// Called from main after fork to register client fd <-> login mapping
void notify_register(int socket_fd, const char *login);
void notify_unregister(int socket_fd);
