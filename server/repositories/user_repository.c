#include <user_repository.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

#define USERS_FILE "data/users.json"

// Opens file, acquires LOCK_EX, reads and parses JSON.
// Returns parsed JSON (caller must cJSON_Delete) and stores fd in *fd_out.
// Caller must call locked_save or close(*fd_out) to release the lock.
static cJSON *locked_load(const char *path, int *fd_out) {
    int fd = open(path, O_RDWR | O_CREAT, 0644);
    if (fd < 0) {
        *fd_out = -1;
        return cJSON_CreateArray();
    }

    flock(fd, LOCK_EX);
    *fd_out = fd;

    off_t len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if (len <= 0) return cJSON_CreateArray();

    char *buf = malloc(len + 1);
    if (!buf) return cJSON_CreateArray();

    read(fd, buf, len);
    buf[len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    free(buf);

    return json ? json : cJSON_CreateArray();
}

// Truncates file, writes JSON, then closes fd (releases flock).
static int locked_save(int fd, cJSON *json) {
    char *str = cJSON_Print(json);
    if (!str) { close(fd); return -1; }

    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);

    ssize_t len = (ssize_t)strlen(str);
    ssize_t written = write(fd, str, len);
    free(str);
    close(fd);  // releases flock

    return (written == len) ? 0 : -1;
}

// Read-only load: shared lock, no fd returned.
static cJSON *load_json_file(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return cJSON_CreateArray();

    flock(fd, LOCK_SH);

    off_t len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    if (len <= 0) { close(fd); return cJSON_CreateArray(); }

    char *buf = malloc(len + 1);
    if (!buf) { close(fd); return cJSON_CreateArray(); }

    read(fd, buf, len);
    buf[len] = '\0';
    close(fd);  // releases flock

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json ? json : cJSON_CreateArray();
}

int repo_user_exists(const char *login) {
    cJSON *arr = load_json_file(USERS_FILE);
    if (!arr) return 0;

    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        cJSON *l = cJSON_GetObjectItem(item, "login");
        if (l && cJSON_IsString(l) && strcmp(l->valuestring, login) == 0) {
            cJSON_Delete(arr);
            return 1;
        }
    }

    cJSON_Delete(arr);
    return 0;
}

int repo_user_create(const char *login, const char *password_hash) {
    int fd;
    cJSON *arr = locked_load(USERS_FILE, &fd);
    if (!arr) return -1;

    cJSON *user = cJSON_CreateObject();
    cJSON_AddStringToObject(user, "login", login);
    cJSON_AddStringToObject(user, "password_hash", password_hash);
    cJSON_AddItemToArray(arr, user);

    int ret = locked_save(fd, arr);
    cJSON_Delete(arr);
    return ret;
}

int repo_user_get_hash(const char *login, char *hash_out) {
    cJSON *arr = load_json_file(USERS_FILE);
    if (!arr) return -1;

    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        cJSON *l = cJSON_GetObjectItem(item, "login");
        if (l && cJSON_IsString(l) && strcmp(l->valuestring, login) == 0) {
            cJSON *h = cJSON_GetObjectItem(item, "password_hash");
            if (h && cJSON_IsString(h)) {
                strcpy(hash_out, h->valuestring);
                cJSON_Delete(arr);
                return 0;
            }
        }
    }

    cJSON_Delete(arr);
    return -1;
}

int repo_user_list(char ***logins_out, int *count) {
    cJSON *arr = load_json_file(USERS_FILE);
    if (!arr) return -1;

    int n = cJSON_GetArraySize(arr);
    char **logins = malloc(sizeof(char *) * n);
    if (!logins) {
        cJSON_Delete(arr);
        return -1;
    }

    int idx = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        cJSON *l = cJSON_GetObjectItem(item, "login");
        if (l && cJSON_IsString(l)) {
            logins[idx] = strdup(l->valuestring);
            idx++;
        }
    }

    *logins_out = logins;
    *count = idx;
    cJSON_Delete(arr);
    return 0;
}
