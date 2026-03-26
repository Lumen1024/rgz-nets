#pragma once

#include "../../shared/protocol.h"

Response handle_get_chat_messages(const char *chat);
Response handle_post_chat_message(const char *chat, Request *req, const char *login);
Response handle_get_private_messages(const char *login_a, const char *login_b);
Response handle_post_private_message(const char *to, Request *req, const char *login);
