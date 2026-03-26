#pragma once

#include <protocol.h>

int repo_msg_save_chat(const char *chat, const char *login, const char *text);
int repo_msg_get_chat(const char *chat, Message **msgs_out, int *count);
int repo_msg_save_private(const char *from, const char *to, const char *text);
int repo_msg_get_private(const char *login_a, const char *login_b, Message **msgs_out, int *count);
