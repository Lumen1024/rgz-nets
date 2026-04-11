#include <chat_repository.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

#define CHATS_FILE "data/chats.json"

static cJSON *locked_load(const char *path, int *fd_out)
{
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0)
    {
        *fd_out = -1;
        return cJSON_CreateArray();
    }

    flock(fd, LOCK_EX);
    *fd_out = fd;

    off_t len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if (len <= 0)
        return cJSON_CreateArray();

    char *buf = malloc(len + 1);
    if (!buf)
        return cJSON_CreateArray();

    read(fd, buf, len);
    buf[len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json ? json : cJSON_CreateArray();
}

static int locked_save(int fd, cJSON *json)
{
    char *str = cJSON_Print(json);
    if (!str)
    {
        close(fd);
        return -1;
    }

    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);

    ssize_t len = (ssize_t)strlen(str);
    ssize_t written = write(fd, str, len);
    free(str);
    close(fd);

    return (written == len) ? 0 : -1;
}

static cJSON *load_json_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return cJSON_CreateArray();

    flock(fd, LOCK_SH);

    off_t len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if (len <= 0)
    {
        close(fd);
        return cJSON_CreateArray();
    }

    char *buf = malloc(len + 1);
    if (!buf)
    {
        close(fd);
        return cJSON_CreateArray();
    }

    read(fd, buf, len);
    buf[len] = '\0';
    close(fd);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json ? json : cJSON_CreateArray();
}

static cJSON *find_chat(cJSON *arr, const char *name)
{
    cJSON *item;
    cJSON_ArrayForEach(item, arr)
    {
        cJSON *n = cJSON_GetObjectItem(item, "name");
        if (n && cJSON_IsString(n) && strcmp(n->valuestring, name) == 0)
        {
            return item;
        }
    }
    return NULL;
}

int repo_chat_create(const char *name, const char *creator)
{
    int fd;
    cJSON *arr = locked_load(CHATS_FILE, &fd);
    if (!arr)
        return -1;

    cJSON *chat = cJSON_CreateObject();
    cJSON_AddStringToObject(chat, "name", name);
    cJSON_AddStringToObject(chat, "creator", creator);

    cJSON *users = cJSON_CreateArray();
    cJSON_AddItemToArray(users, cJSON_CreateString(creator));
    cJSON_AddItemToObject(chat, "users", users);

    cJSON_AddItemToArray(arr, chat);

    int ret = locked_save(fd, arr);
    cJSON_Delete(arr);
    return ret;
}

int repo_chat_delete(const char *name)
{
    int fd;
    cJSON *arr = locked_load(CHATS_FILE, &fd);
    if (!arr)
        return -1;

    int size = cJSON_GetArraySize(arr);
    for (int i = 0; i < size; i++)
    {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        cJSON *n = cJSON_GetObjectItem(item, "name");
        if (n && cJSON_IsString(n) && strcmp(n->valuestring, name) == 0)
        {
            cJSON_DeleteItemFromArray(arr, i);
            int ret = locked_save(fd, arr);
            cJSON_Delete(arr);
            return ret;
        }
    }

    close(fd);
    cJSON_Delete(arr);
    return -1;
}

int repo_chat_exists(const char *name)
{
    cJSON *arr = load_json_file(CHATS_FILE);
    if (!arr)
        return 0;

    cJSON *chat = find_chat(arr, name);
    int exists = (chat != NULL) ? 1 : 0;

    cJSON_Delete(arr);
    return exists;
}

int repo_chat_get_host(const char *name, char *host_out)
{
    cJSON *arr = load_json_file(CHATS_FILE);
    if (!arr)
        return -1;

    cJSON *chat = find_chat(arr, name);
    if (!chat)
    {
        cJSON_Delete(arr);
        return -1;
    }

    cJSON *creator = cJSON_GetObjectItem(chat, "creator");
    if (!creator || !cJSON_IsString(creator))
    {
        cJSON_Delete(arr);
        return -1;
    }

    strcpy(host_out, creator->valuestring);
    cJSON_Delete(arr);
    return 0;
}

int repo_chat_list_for_user(const char *login, char ***names_out, int *count)
{
    cJSON *arr = load_json_file(CHATS_FILE);
    if (!arr)
        return -1;

    int size = cJSON_GetArraySize(arr);
    char **names = malloc(sizeof(char *) * size);
    if (!names)
    {
        cJSON_Delete(arr);
        return -1;
    }

    int idx = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, arr)
    {
        cJSON *users = cJSON_GetObjectItem(item, "users");
        if (!users)
            continue;

        cJSON *u;
        cJSON_ArrayForEach(u, users)
        {
            if (cJSON_IsString(u) && strcmp(u->valuestring, login) == 0)
            {
                cJSON *n = cJSON_GetObjectItem(item, "name");
                if (n && cJSON_IsString(n))
                {
                    names[idx] = strdup(n->valuestring);
                    idx++;
                }
                break;
            }
        }
    }

    *names_out = names;
    *count = idx;
    cJSON_Delete(arr);
    return 0;
}

int repo_chat_add_user(const char *chat, const char *login)
{
    int fd;
    cJSON *arr = locked_load(CHATS_FILE, &fd);
    if (!arr)
        return -1;

    cJSON *c = find_chat(arr, chat);
    if (!c)
    {
        close(fd);
        cJSON_Delete(arr);
        return -1;
    }

    cJSON *users = cJSON_GetObjectItem(c, "users");
    if (!users)
    {
        close(fd);
        cJSON_Delete(arr);
        return -1;
    }

    cJSON *u;
    cJSON_ArrayForEach(u, users)
    {
        if (cJSON_IsString(u) && strcmp(u->valuestring, login) == 0)
        {
            close(fd);
            cJSON_Delete(arr);
            return 0;
        }
    }

    cJSON_AddItemToArray(users, cJSON_CreateString(login));

    int ret = locked_save(fd, arr);
    cJSON_Delete(arr);
    return ret;
}

int repo_chat_remove_user(const char *chat, const char *login)
{
    int fd;
    cJSON *arr = locked_load(CHATS_FILE, &fd);
    if (!arr)
        return -1;

    cJSON *c = find_chat(arr, chat);
    if (!c)
    {
        close(fd);
        cJSON_Delete(arr);
        return -1;
    }

    cJSON *users = cJSON_GetObjectItem(c, "users");
    if (!users)
    {
        close(fd);
        cJSON_Delete(arr);
        return -1;
    }

    int size = cJSON_GetArraySize(users);
    for (int i = 0; i < size; i++)
    {
        cJSON *u = cJSON_GetArrayItem(users, i);
        if (cJSON_IsString(u) && strcmp(u->valuestring, login) == 0)
        {
            cJSON_DeleteItemFromArray(users, i);
            int ret = locked_save(fd, arr);
            cJSON_Delete(arr);
            return ret;
        }
    }

    close(fd);
    cJSON_Delete(arr);
    return -1;
}

int repo_chat_list_users(const char *chat, char ***logins_out, int *count)
{
    cJSON *arr = load_json_file(CHATS_FILE);
    if (!arr)
        return -1;

    cJSON *c = find_chat(arr, chat);
    if (!c)
    {
        cJSON_Delete(arr);
        return -1;
    }

    cJSON *users = cJSON_GetObjectItem(c, "users");
    if (!users)
    {
        cJSON_Delete(arr);
        return -1;
    }

    int size = cJSON_GetArraySize(users);
    char **logins = malloc(sizeof(char *) * size);
    if (!logins)
    {
        cJSON_Delete(arr);
        return -1;
    }

    int idx = 0;
    cJSON *u;
    cJSON_ArrayForEach(u, users)
    {
        if (cJSON_IsString(u))
        {
            logins[idx] = strdup(u->valuestring);
            idx++;
        }
    }

    *logins_out = logins;
    *count = idx;
    cJSON_Delete(arr);
    return 0;
}
