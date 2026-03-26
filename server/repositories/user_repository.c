#include <user_repository.h>
#include <cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USERS_FILE "data/users.json"

static cJSON *load_json_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return cJSON_CreateArray();

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len <= 0) {
        fclose(f);
        return cJSON_CreateArray();
    }

    char *buf = malloc(len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);

    cJSON *json = cJSON_Parse(buf);
    free(buf);

    return json ? json : cJSON_CreateArray();
}

static int save_json_file(const char *path, cJSON *json) {
    char *str = cJSON_Print(json);
    if (!str) return -1;

    FILE *f = fopen(path, "w");
    if (!f) {
        free(str);
        return -1;
    }

    fprintf(f, "%s", str);
    fclose(f);
    free(str);
    return 0;
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
    cJSON *arr = load_json_file(USERS_FILE);
    if (!arr) return -1;

    cJSON *user = cJSON_CreateObject();
    cJSON_AddStringToObject(user, "login", login);
    cJSON_AddStringToObject(user, "password_hash", password_hash);
    cJSON_AddItemToArray(arr, user);

    int ret = save_json_file(USERS_FILE, arr);
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
