#pragma once

int repo_chat_create(const char *name, const char *creator);
int repo_chat_delete(const char *name);
int repo_chat_exists(const char *name);
int repo_chat_get_host(const char *name, char *host_out);
int repo_chat_list_for_user(const char *login, char ***names_out, int *count);
int repo_chat_add_user(const char *chat, const char *login);
int repo_chat_remove_user(const char *chat, const char *login);
int repo_chat_list_users(const char *chat, char ***logins_out, int *count);
