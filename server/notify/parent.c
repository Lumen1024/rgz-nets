#define _POSIX_C_SOURCE 200809L

#include <notify/parent.h>
#include <notify/shared.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <socket_utils.h>
#include <protocol.h>

void notify_parent_register(pid_t pid, int pipe_read_fd, int client_fd)
{
    if (!g_shared)
        return;
    pthread_mutex_lock(&g_shared->lock);
    if (g_shared->count < MAX_CLIENTS)
    {
        ClientEntry *e = &g_shared->entries[g_shared->count++];
        e->pid = pid;
        e->client_fd = client_fd;
        e->pipe_read_fd = pipe_read_fd;
        e->login[0] = '\0';
    }
    pthread_mutex_unlock(&g_shared->lock);
}

void notify_parent_unregister(pid_t pid)
{
    if (!g_shared)
        return;
    pthread_mutex_lock(&g_shared->lock);
    for (int i = 0; i < g_shared->count; i++)
    {
        if (g_shared->entries[i].pid == pid)
        {
            close(g_shared->entries[i].pipe_read_fd);
            close(g_shared->entries[i].client_fd);
            g_shared->entries[i] = g_shared->entries[--g_shared->count];
            break;
        }
    }
    pthread_mutex_unlock(&g_shared->lock);
}

void notify_dispatch()
{
    if (!g_shared)
        return;

    pthread_mutex_lock(&g_shared->lock);
    int count = g_shared->count;
    if (count == 0)
    {
        pthread_mutex_unlock(&g_shared->lock);
        return;
    }

    fd_set rfds;
    FD_ZERO(&rfds);
    int maxfd = -1;
    for (int i = 0; i < count; i++)
    {
        int fd = g_shared->entries[i].pipe_read_fd;
        FD_SET(fd, &rfds);
        if (fd > maxfd)
            maxfd = fd;
    }
    pthread_mutex_unlock(&g_shared->lock);

    struct timeval tv = {0, 0};
    if (select(maxfd + 1, &rfds, NULL, NULL, &tv) <= 0)
        return;

    pthread_mutex_lock(&g_shared->lock);
    for (int i = 0; i < g_shared->count; i++)
    {
        int pfd = g_shared->entries[i].pipe_read_fd;
        if (!FD_ISSET(pfd, &rfds))
            continue;

        char hdr[PIPE_HEADER_SIZE];
        if (read(pfd, hdr, PIPE_HEADER_SIZE) != (ssize_t)PIPE_HEADER_SIZE)
            continue;

        char login[MAX_LOGIN_LEN];
        memcpy(login, hdr, MAX_LOGIN_LEN);
        login[MAX_LOGIN_LEN - 1] = '\0';

        uint32_t json_len;
        memcpy(&json_len, hdr + MAX_LOGIN_LEN, sizeof(uint32_t));
        if (json_len == 0 || json_len > MSG_BUFFER_SIZE)
            continue;

        char *json = malloc(json_len + 1);
        if (!json)
            continue;

        ssize_t got = read(pfd, json, json_len);
        json[json_len] = '\0';

        if (got == (ssize_t)json_len)
        {
            int cfd = -1;
            for (int j = 0; j < g_shared->count; j++)
            {
                if (strcmp(g_shared->entries[j].login, login) == 0)
                {
                    cfd = g_shared->entries[j].client_fd;
                    break;
                }
            }

            if (cfd >= 0)
            {
                printf("[notify_dispatch] -> %s (fd=%d)\n", login, cfd);
                fflush(stdout);
                write_message(cfd, json);
            }
            else
            {
                printf("[notify_dispatch] %s not connected\n", login);
                fflush(stdout);
            }
        }
        free(json);
    }
    pthread_mutex_unlock(&g_shared->lock);
}
