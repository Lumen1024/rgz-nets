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

typedef struct
{
    pid_t pid;
    int client_fd;
    int pipe_read_fd;
} ParentEntry;

static ParentEntry g_parent[MAX_CLIENTS];
static int g_parent_count = 0;

void notify_parent_register(pid_t pid, int pipe_read_fd, int client_fd)
{
    if (g_parent_count >= MAX_CLIENTS)
        return;
    g_parent[g_parent_count].pid = pid;
    g_parent[g_parent_count].pipe_read_fd = pipe_read_fd;
    g_parent[g_parent_count].client_fd = client_fd;
    g_parent_count++;
}

void notify_parent_unregister(pid_t pid)
{
    for (int i = 0; i < g_parent_count; i++)
    {
        if (g_parent[i].pid == pid)
        {
            close(g_parent[i].pipe_read_fd);
            close(g_parent[i].client_fd);
            g_parent[i] = g_parent[--g_parent_count];
            return;
        }
    }
}

static int find_client_fd(const char *login)
{
    if (!g_shared)
        return -1;

    pthread_mutex_lock(&g_shared->lock);
    pid_t target_pid = -1;
    for (int i = 0; i < g_shared->count; i++)
    {
        if (strcmp(g_shared->entries[i].login, login) == 0)
        {
            target_pid = g_shared->entries[i].pid;
            break;
        }
    }
    pthread_mutex_unlock(&g_shared->lock);

    if (target_pid < 0)
        return -1;

    for (int i = 0; i < g_parent_count; i++)
    {
        if (g_parent[i].pid == target_pid)
            return g_parent[i].client_fd;
    }
    return -1;
}

void notify_dispatch()
{
    if (g_parent_count == 0)
        return;

    fd_set rfds;
    FD_ZERO(&rfds);
    int maxfd = -1;

    for (int i = 0; i < g_parent_count; i++)
    {
        int fd = g_parent[i].pipe_read_fd;
        FD_SET(fd, &rfds);
        if (fd > maxfd)
            maxfd = fd;
    }

    struct timeval tv = {0, 0};
    if (select(maxfd + 1, &rfds, NULL, NULL, &tv) <= 0)
        return;

    for (int i = 0; i < g_parent_count; i++)
    {
        int pfd = g_parent[i].pipe_read_fd;
        if (!FD_ISSET(pfd, &rfds))
            continue;

        char hdr[PIPE_HDR_SIZE];
        if (read(pfd, hdr, PIPE_HDR_SIZE) != (ssize_t)PIPE_HDR_SIZE)
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
            int cfd = find_client_fd(login);
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
}
