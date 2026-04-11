#pragma once

#include <protocol.h>
#include <model.h>

int connect_to_server(const char *host, int port);

int api_get_chat_messages(const char *route, Message *msgs_out, int max, int *count_out);
int api_get_chat_list(char names_out[][MAX_ROUTE_LEN], int max, int *count_out);
int api_get_user_list(char names_out[][MAX_LOGIN_LEN], int max, int *count_out);
int api_get_member_list(const char *chat_name, char names_out[][MAX_LOGIN_LEN], int max, int *count_out);

int         api_login(const char *login, const char *password);
int         api_register(const char *login, const char *password);
const char *api_get_login();
int api_send_message(const char *route, const char *text);
int api_create_chat(const char *name);
int api_add_chat_user(const char *chat, const char *login);
int api_remove_chat_user(const char *chat, const char *login);
int api_leave_chat(const char *chat);
int api_send_file(const char *to, const char *filepath);
