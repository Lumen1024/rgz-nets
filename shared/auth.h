#pragma once

char *hash_password(const char *password);

char *generate_token(const char *login);
int validate_token(const char *token, char *login_out);
