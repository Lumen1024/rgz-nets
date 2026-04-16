#pragma once

#include <protocol.h>

void notify_child_init(int pipe_write_fd);
void notify_register(const char *login);

void notify_user(const char *login, Notification notif);
void notify_chat(const char *chat_name, Notification notif);

void notify_new_chat_message(const char *chat, const char *login, const char *text);
void notify_new_private_message(const char *to, const char *from, const char *text);
