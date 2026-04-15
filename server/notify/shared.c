#define _POSIX_C_SOURCE 200809L

#include <notify/shared.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

SharedClients *g_shared = NULL;

void notify_shared_init()
{
    g_shared = mmap(NULL, sizeof(SharedClients),
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (g_shared == MAP_FAILED)
    {
        perror("mmap");
        exit(1);
    }

    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&g_shared->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    g_shared->count = 0;
}
