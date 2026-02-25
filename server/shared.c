#include <shared.h>
#include <sys/mman.h>
#include <string.h>

Shared *shared_init(void)
{
    // общая память для клиентских процессов
    Shared *sh = mmap(
        NULL,                   /* адрес: ядро само выберет подходящий адрес */
        sizeof(Shared),         /* размер: сколько байт выделить             */
        PROT_READ | PROT_WRITE, /* права: можно читать и записывать      */
        MAP_SHARED              /* изменения видны всем процессам (fork)     */
            | MAP_ANONYMOUS,    /* не привязана к файлу, только в RAM        */
        -1,                     /* fd: -1 обязателен для MAP_ANONYMOUS       */
        0                       /* offset: смещение в файле, для анон. = 0   */
    );

    sem_init(&sh->mutex, 1, 1);
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
