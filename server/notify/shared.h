#pragma once

#include <pthread.h>
#include <sys/types.h>
#include <model.h>
#include <protocol.h>

#define PIPE_HDR_SIZE (MAX_LOGIN_LEN + sizeof(uint32_t))
#define MAX_CLIENTS 256

typedef struct
{
    pid_t pid;
    char login[MAX_LOGIN_LEN];
} SharedEntry;

typedef struct
{
    pthread_mutex_t lock;
    int count;
    SharedEntry entries[MAX_CLIENTS];
} SharedClients;

extern SharedClients *g_shared;

void notify_shared_init();
