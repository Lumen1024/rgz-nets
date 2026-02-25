#pragma once

#include <vars.h>

typedef struct ServerInfo
{
    char ip[32];
    char name[NAME_LEN];
} ServerInfo;

/* Сканирует локальную сеть, заполняет массив out.
   Возвращает количество найденных серверов. */
int scan_local_network(ServerInfo *out, int max_count);