#pragma once
#include <shared.h>

typedef struct
{
    int fd;
    Shared *sh;
    char srv_name[NAME_LEN];
} ClientCtx;

void handle_client(ClientCtx ctx);
