#pragma once
#include <semaphore.h>
#include <vars.h>

typedef struct
{
    sem_t mutex;
    int total;
    char msgs[MAX_MSGS][FMT_MSG_LEN];
} Shared;

Shared *shared_init(void);

void shared_broadcast(Shared *sh, const char *msg);
