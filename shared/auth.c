#include <auth.h>
#include <protocol.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SECRET_KEY "rgz_secret_key"

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

static unsigned long sign(const char *login, long timestamp)
{
    char buf[512];
    snprintf(buf, sizeof(buf), "%s:%ld:%s", login, timestamp, SECRET_KEY);
    return djb2(buf);
}

char *hash_password(const char *password)
{
    unsigned long h = djb2(password);
    char *hex = malloc(17);
    if (!hex)
        return NULL;
    snprintf(hex, 17, "%016lx", h);
    return hex;
}

char *generate_token(const char *login)
{
    // Format: "login:timestamp:signature"
    time_t now = time(NULL);
    unsigned long sig = sign(login, (long)now);

    size_t len = strlen(login) + 1 + 20 + 1 + 16 + 1;
    char *token = malloc(len);
    if (!token)
        return NULL;
    snprintf(token, len, "%s:%ld:%016lx", login, (long)now, sig);
    return token;
}

int validate_token(const char *token, char *login_out)
{
    if (!token || !login_out)
        return -1;

    // find second colon from the right (signature starts after it)
    const char *sig_colon = strrchr(token, ':');
    if (!sig_colon || sig_colon == token)
        return -1;

    // find first colon (separates login from timestamp)
    const char *ts_colon = strchr(token, ':');
    if (!ts_colon || ts_colon == sig_colon)
        return -1;

    size_t login_len = (size_t)(ts_colon - token);
    long timestamp = atol(ts_colon + 1);
    unsigned long stored_sig = strtoul(sig_colon + 1, NULL, 16);

    char login_buf[MAX_LOGIN_LEN];
    if (login_len >= MAX_LOGIN_LEN)
        return -1;
    memcpy(login_buf, token, login_len);
    login_buf[login_len] = '\0';

    if (sign(login_buf, timestamp) != stored_sig)
        return -1;

    memcpy(login_out, login_buf, login_len + 1);
    return 0;
}
