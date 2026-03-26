#include <message_repository.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

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

static int locked_save(int fd, cJSON *json) {
    char *str = cJSON_Print(json);
    if (!str) { close(fd); return -1; }

    ftruncate(fd, 0);
    lseek(fd, 0, SEEK_SET);

    ssize_t len = (ssize_t)strlen(str);
    ssize_t written = write(fd, str, len);
    free(str);
    close(fd);

    return (written == len) ? 0 : -1;
}

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
    close(fd);

    cJSON *json = cJSON_Parse(buf);
    free(buf);
    return json ? json : cJSON_CreateArray();
}

static void ensure_dirs(const char *path) {
    char tmp[512];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
}

static void get_private_path(const char *a, const char *b, char *out, size_t out_size) {
    const char *first, *second;
    if (strcmp(a, b) < 0) {
        first = a;
        second = b;
    } else {
        first = b;
        second = a;
    }
    snprintf(out, out_size, "data/messages/private/%s-%s.json", first, second);
}

static void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    strftime(buf, size, "%Y-%m-%d %H:%M:%S", tm);
}

static cJSON *create_message_json(const char *login, const char *text) {
    char ts[64];
    get_timestamp(ts, sizeof(ts));

    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "login", login);
    cJSON_AddStringToObject(msg, "text", text);
    cJSON_AddStringToObject(msg, "timestamp", ts);
    return msg;
}

static int parse_messages(cJSON *arr, Message **msgs_out, int *count) {
    int n = cJSON_GetArraySize(arr);
    Message *msgs = malloc(sizeof(Message) * (n > 0 ? n : 1));
    if (!msgs) return -1;

    int idx = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, arr) {
        cJSON *l = cJSON_GetObjectItem(item, "login");
        cJSON *t = cJSON_GetObjectItem(item, "text");
        cJSON *ts = cJSON_GetObjectItem(item, "timestamp");

        if (l && cJSON_IsString(l)) {
            strncpy(msgs[idx].login, l->valuestring, sizeof(msgs[idx].login) - 1);
            msgs[idx].login[sizeof(msgs[idx].login) - 1] = '\0';
        }
        if (t && cJSON_IsString(t)) {
            strncpy(msgs[idx].text, t->valuestring, sizeof(msgs[idx].text) - 1);
            msgs[idx].text[sizeof(msgs[idx].text) - 1] = '\0';
        }
        if (ts && cJSON_IsString(ts)) {
            strncpy(msgs[idx].timestamp, ts->valuestring, sizeof(msgs[idx].timestamp) - 1);
            msgs[idx].timestamp[sizeof(msgs[idx].timestamp) - 1] = '\0';
        }
        idx++;
    }

    *msgs_out = msgs;
    *count = idx;
    return 0;
}

int repo_msg_save_chat(const char *chat, const char *login, const char *text) {
    char path[512];
    snprintf(path, sizeof(path), "data/messages/chat/%s.json", chat);
    ensure_dirs(path);

    int fd;
    cJSON *arr = locked_load(path, &fd);
    if (!arr) return -1;

    cJSON *msg = create_message_json(login, text);
    cJSON_AddItemToArray(arr, msg);

    int ret = locked_save(fd, arr);
    cJSON_Delete(arr);
    return ret;
}

int repo_msg_get_chat(const char *chat, Message **msgs_out, int *count) {
    char path[512];
    snprintf(path, sizeof(path), "data/messages/chat/%s.json", chat);

    cJSON *arr = load_json_file(path);
    if (!arr) return -1;

    int ret = parse_messages(arr, msgs_out, count);
    cJSON_Delete(arr);
    return ret;
}

int repo_msg_save_private(const char *from, const char *to, const char *text) {
    char path[512];
    get_private_path(from, to, path, sizeof(path));
    ensure_dirs(path);

    int fd;
    cJSON *arr = locked_load(path, &fd);
    if (!arr) return -1;

    cJSON *msg = create_message_json(from, text);
    cJSON_AddItemToArray(arr, msg);

    int ret = locked_save(fd, arr);
    cJSON_Delete(arr);
    return ret;
}

int repo_msg_get_private(const char *login_a, const char *login_b, Message **msgs_out, int *count) {
    char path[512];
    get_private_path(login_a, login_b, path, sizeof(path));

    cJSON *arr = load_json_file(path);
    if (!arr) return -1;

    int ret = parse_messages(arr, msgs_out, count);
    cJSON_Delete(arr);
    return ret;
}
