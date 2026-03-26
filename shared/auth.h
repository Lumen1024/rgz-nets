#pragma once

char *hash_password(const char *password);
int verify_password(const char *password, const char *hash);
char *generate_token(const char *login);
int validate_token(const char *token, char *login_out);
