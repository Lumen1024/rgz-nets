#include "shared.h"
#include <sys/mman.h>
#include <string.h>

Shared *shared_init(void)
{
    Shared *sh = mmap(NULL, sizeof(Shared),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sh == MAP_FAILED) return NULL;
    sem_init(&sh->mutex, /*pshared=*/1, 1);
    sh->total = 0;
    return sh;
}

void shared_broadcast(Shared *sh, const char *msg)
{
    sem_wait(&sh->mutex);
    int idx = sh->total % MAX_MSGS;
    strncpy(sh->msgs[idx], msg, MSG_LEN - 1);
    sh->msgs[idx][MSG_LEN - 1] = '\0';
    sh->total++;
    sem_post(&sh->mutex);
}
