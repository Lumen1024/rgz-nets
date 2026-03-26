#pragma once

int repo_user_exists(const char *login);
int repo_user_create(const char *login, const char *password_hash);
int repo_user_get_hash(const char *login, char *hash_out);
int repo_user_list(char ***logins_out, int *count);
