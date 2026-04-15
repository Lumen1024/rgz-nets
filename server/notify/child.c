#define _POSIX_C_SOURCE 200809L

#include <notify/child.h>
#include <notify/shared.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <cJSON.h>
#include <notification.h>
#include <chat_repository.h>

static int g_child_pipe_write = -1;

void notify_child_init(int pipe_write_fd)
{
    g_child_pipe_write = pipe_write_fd;
}

void notify_register(int socket_fd, const char *login)
{
    (void)socket_fd;
    if (!g_shared)
        return;
    pid_t my_pid = getpid();
    pthread_mutex_lock(&g_shared->lock);

    for (int i = 0; i < g_shared->count; i++)
    {
        if (g_shared->entries[i].pid == my_pid)
        {
            strncpy(g_shared->entries[i].login, login, MAX_LOGIN_LEN - 1);
            g_shared->entries[i].login[MAX_LOGIN_LEN - 1] = '\0';
            pthread_mutex_unlock(&g_shared->lock);
            return;
        }
    }

    if (g_shared->count < MAX_CLIENTS)
    {
        g_shared->entries[g_shared->count].pid = my_pid;
        strncpy(g_shared->entries[g_shared->count].login, login, MAX_LOGIN_LEN - 1);
        g_shared->entries[g_shared->count].login[MAX_LOGIN_LEN - 1] = '\0';
        g_shared->count++;
    }

    pthread_mutex_unlock(&g_shared->lock);
}

void notify_unregister(int socket_fd)
{
    (void)socket_fd;
    if (!g_shared)
        return;
    pid_t my_pid = getpid();
    pthread_mutex_lock(&g_shared->lock);

    for (int i = 0; i < g_shared->count; i++)
    {
        if (g_shared->entries[i].pid == my_pid)
        {
            g_shared->entries[i] = g_shared->entries[--g_shared->count];
            break;
        }
    }

    pthread_mutex_unlock(&g_shared->lock);
}

void notify_user(const char *login, Notification notif)
{
    if (g_child_pipe_write < 0)
        return;

    cJSON *obj = cJSON_CreateObject();
    cJSON_AddStringToObject(obj, "kind", "notification");
    cJSON_AddNumberToObject(obj, "code", notif.code);
    if (notif.content)
        cJSON_AddItemReferenceToObject(obj, "content", notif.content);
    char *json = cJSON_PrintUnformatted(obj);
    cJSON_Delete(obj);
    if (!json)
        return;

    uint32_t json_len = (uint32_t)strlen(json);

    char hdr[PIPE_HDR_SIZE];
    memset(hdr, 0, sizeof(hdr));
    strncpy(hdr, login, MAX_LOGIN_LEN - 1);
    memcpy(hdr + MAX_LOGIN_LEN, &json_len, sizeof(uint32_t));

    write(g_child_pipe_write, hdr, PIPE_HDR_SIZE);
    write(g_child_pipe_write, json, json_len);
    free(json);
}

void notify_chat(const char *chat_name, Notification notif)
{
    char **logins = NULL;
    int count = 0;
    if (repo_chat_list_users(chat_name, &logins, &count) != 0)
        return;

    for (int i = 0; i < count; i++)
    {
        notify_user(logins[i], notif);
        free(logins[i]);
    }
    free(logins);
}
