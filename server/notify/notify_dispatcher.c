#define _POSIX_C_SOURCE 200809L

#include <notify/notify_dispatcher.h>
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

void notify_client_add(pid_t pid, int pipe_read_fd, int client_fd)
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

void notify_client_remove(pid_t pid)
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

// Find client_fd by login in g_shared (must be called with lock held)
static int find_client_fd(const char *login)
{
    for (int i = 0; i < g_shared->count; i++)
    {
        if (strcmp(g_shared->entries[i].login, login) == 0)
            return g_shared->entries[i].client_fd;
    }
    return -1;
}

// Read one notification from pipe and deliver it to the target client
static void dispatch_pipe(int pfd)
{
    char header[PIPE_HEADER_SIZE];
    if (read(pfd, header, PIPE_HEADER_SIZE) != (ssize_t)PIPE_HEADER_SIZE)
        return;

    char login[MAX_LOGIN_LEN];
    memcpy(login, header, MAX_LOGIN_LEN);
    login[MAX_LOGIN_LEN - 1] = '\0';

    uint32_t json_len;
    memcpy(&json_len, header + MAX_LOGIN_LEN, sizeof(uint32_t));
    if (json_len == 0 || json_len > MSG_BUFFER_SIZE)
        return;

    char *json = malloc(json_len + 1);
    if (!json)
        return;

    ssize_t got = read(pfd, json, json_len);
    json[json_len] = '\0';

    if (got == (ssize_t)json_len)
    {
        int cfd = find_client_fd(login);
        if (cfd >= 0)
        {
            printf("[notify] -> %s (fd=%d)\n", login, cfd);
            fflush(stdout);
            write_message(cfd, json);
        }
        else
        {
            printf("[notify] %s not connected\n", login);
            fflush(stdout);
        }
    }
    free(json);
}

void notify_dispatch()
{
    if (!g_shared)
        return;

    // Build fd_set from all registered pipe_read_fds
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

    // Check which pipes have data (non-blocking)
    struct timeval tv = {0, 0};
    if (select(maxfd + 1, &rfds, NULL, NULL, &tv) <= 0)
        return;

    // Deliver notifications from pipes that are ready
    pthread_mutex_lock(&g_shared->lock);
    for (int i = 0; i < g_shared->count; i++)
    {
        int pfd = g_shared->entries[i].pipe_read_fd;
        if (FD_ISSET(pfd, &rfds))
            dispatch_pipe(pfd);
    }
    pthread_mutex_unlock(&g_shared->lock);
}
