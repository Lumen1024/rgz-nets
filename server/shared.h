#pragma once
#include <semaphore.h>
#include "../common/protocol.h"

typedef struct {
    sem_t mutex;
    int   total;
    char  msgs[MAX_MSGS][MSG_LEN];
} Shared;

/* Инициализирует разделяемую память (mmap). Возвращает NULL при ошибке. */
Shared *shared_init(void);

/* Добавляет сообщение в кольцевой буфер. */
void    shared_broadcast(Shared *sh, const char *msg);
