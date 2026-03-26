#pragma once

#include "../../shared/protocol.h"

Response handle_get_chats(const char *login);
Response handle_create_chat(Request *req, const char *login);
Response handle_delete_chat(const char *name, const char *login);
Response handle_get_chat_host(const char *name);
Response handle_get_chat_users(const char *name);
Response handle_add_chat_user(const char *name, Request *req);
Response handle_remove_chat_user(const char *name, Request *req);
