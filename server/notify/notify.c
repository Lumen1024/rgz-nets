#define _POSIX_C_SOURCE 200809L

#include <notify/notify.h>
#include <notify/shared.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <cJSON.h>
#include <notification.h>
#include <protocol.h>
#include <chat_repository.h>

static int g_child_pipe_write = -1;

void notify_child_init(int pipe_write_fd)
{
    g_child_pipe_write = pipe_write_fd;
}

void notify_register(const char *login)
{
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

    char header[PIPE_HEADER_SIZE];
    memset(header, 0, sizeof(header));
    strncpy(header, login, MAX_LOGIN_LEN - 1);
    memcpy(header + MAX_LOGIN_LEN, &json_len, sizeof(uint32_t));

    write(g_child_pipe_write, header, PIPE_HEADER_SIZE);
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

static void fill_timestamp(cJSON *obj)
{
    char ts[MAX_TIMESTAMP_LEN];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    cJSON_AddStringToObject(obj, "timestamp", ts);
}

void notify_new_chat_message(const char *chat, const char *login, const char *text)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", login);
    cJSON_AddStringToObject(body, "text", text);
    cJSON_AddStringToObject(body, "chat", chat);
    fill_timestamp(body);

    Notification notif = {MSG_NOTIFICATION, NOTIF_NEW_MESSAGE, body};
    notify_chat(chat, notif);
    cJSON_Delete(body);
}

void notify_new_private_message(const char *to, const char *from, const char *text)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", from);
    cJSON_AddStringToObject(body, "text", text);
    cJSON_AddStringToObject(body, "to", to);
    fill_timestamp(body);

    Notification notif = {MSG_NOTIFICATION, NOTIF_NEW_MESSAGE, body};
    notify_user(to, notif);
    notify_user(from, notif);
    cJSON_Delete(body);
}
