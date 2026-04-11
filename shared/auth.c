#include <auth.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// djb2 hash algorithm
static unsigned long djb2(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
    {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash;
}

char *hash_password(const char *password)
{
    unsigned long h = djb2(password);
    // 16 hex chars + null terminator
    char *hex = malloc(17);
    if (!hex)
        return NULL;
    snprintf(hex, 17, "%016lx", h);
    return hex;
}

int verify_password(const char *password, const char *hash)
{
    char *computed = hash_password(password);
    if (!computed)
        return -1;
    int result = strcmp(computed, hash) == 0 ? 0 : -1;
    free(computed);
    return result;
}

char *generate_token(const char *login)
{
    // Format: "login:timestamp"
    time_t now = time(NULL);
    size_t len = strlen(login) + 1 + 20 + 1; // login + ':' + timestamp + '\0'
    char *token = malloc(len);
    if (!token)
        return NULL;
    snprintf(token, len, "%s:%ld", login, (long)now);
    return token;
}

int validate_token(const char *token, char *login_out)
{
    if (!token || !login_out)
        return -1;

    const char *colon = strrchr(token, ':');
    if (!colon || colon == token)
        return -1;

    size_t login_len = (size_t)(colon - token);
    memcpy(login_out, token, login_len);
    login_out[login_len] = '\0';

    return 0;
}
