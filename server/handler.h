#pragma once
#include <shared.h>

/* Контекст, который main передаёт дочернему процессу. */
typedef struct
{
    int fd;
    Shared *sh;
    char srv_name[NAME_LEN];
} ClientCtx;

/* Вызывается в дочернем процессе. Внутри — exit(). */
void handle_client(ClientCtx ctx);
