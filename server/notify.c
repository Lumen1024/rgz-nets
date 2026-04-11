#define _POSIX_C_SOURCE 200809L

#include <notify.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <cJSON.h>
#include <notification.h>
#include <socket_utils.h>
#include <chat_repository.h>

// ── Pipe message format ───────────────────────────────────────────────────────
// Each message written by child to its pipe write-end:
//   char     login[MAX_LOGIN_LEN]  — recipient (null-padded, fixed size)
//   uint32_t json_len              — byte length of the JSON string below
//   char     json[json_len]        — cJSON_PrintUnformatted of the notification

#define PIPE_HDR_SIZE (MAX_LOGIN_LEN + sizeof(uint32_t))

// ── Shared memory: pid → login ────────────────────────────────────────────────

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

static SharedClients *g_shared = NULL;

// ── Parent-side table: pid → {client_fd, pipe_read_fd} ───────────────────────

typedef struct
{
    pid_t pid;
    int client_fd;
    int pipe_read_fd;
} ParentEntry;

static ParentEntry g_parent[MAX_CLIENTS];
static int g_parent_count = 0;

// ── Child-side write-end of its pipe ─────────────────────────────────────────

static int g_child_pipe_write = -1;

// ─────────────────────────────────────────────────────────────────────────────

void notify_init()
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

void notify_child_init(int pipe_write_fd)
{
    g_child_pipe_write = pipe_write_fd;
}

// ── Child: register login ─────────────────────────────────────────────────────

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

// ── Child: send notification request to parent via pipe ──────────────────────

void notify_user(const char *login, Notification notif)
{
    if (g_child_pipe_write < 0)
        return;

    // Serialise notification
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

// ── Parent: find client_fd by login ──────────────────────────────────────────

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

// ── Parent: poll pipes and forward notifications ──────────────────────────────

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
