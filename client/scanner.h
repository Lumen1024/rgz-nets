#pragma once
#include "../common/protocol.h"

typedef struct {
    char ip[32];
    char name[NAME_LEN];
} ServerEntry;

/* Сканирует локальную сеть, заполняет массив out.
   Возвращает количество найденных серверов. */
int scan_local_network(ServerEntry *out, int max_count);