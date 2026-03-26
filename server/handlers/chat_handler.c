#include "chat_handler.h"

#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "../../shared/protocol.h"
#include "../../shared/response.h"
#include "../repositories/chat_repository.h"
#include "../repositories/user_repository.h"

Response handle_get_chats(const char *login) {
    char **names = NULL;
    int count = 0;

    if (repo_chat_list_for_user(login, &names, &count) != 0) {
        return make_error(1);
    }

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(names[i]));
        free(names[i]);
    }
    free(names);

    return make_success(arr);
}

Response handle_create_chat(Request *req, const char *login) {
    if (!req->content) {
        return make_error(1);
    }

    cJSON *name_j = cJSON_GetObjectItem(req->content, "name");
    if (!cJSON_IsString(name_j) || strlen(name_j->valuestring) == 0) {
        return make_error(1);
    }

    const char *name = name_j->valuestring;

    if (repo_chat_exists(name)) {
        return make_error(5);
    }

    if (repo_chat_create(name, login) != 0) {
        return make_error(1);
    }

    // Add the creator as the first member
    repo_chat_add_user(name, login);

    return make_success(NULL);
}

Response handle_delete_chat(const char *name, const char *login) {
    if (!repo_chat_exists(name)) {
        return make_error(2);
    }

    char host[256];
    if (repo_chat_get_host(name, host) != 0) {
        return make_error(1);
    }

    if (strcmp(host, login) != 0) {
        return make_error(4);
    }

    if (repo_chat_delete(name) != 0) {
        return make_error(1);
    }

    return make_success(NULL);
}

Response handle_get_chat_host(const char *name) {
    if (!repo_chat_exists(name)) {
        return make_error(2);
    }

    char host[256];
    if (repo_chat_get_host(name, host) != 0) {
        return make_error(1);
    }

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", host);

    return make_success(body);
}

Response handle_get_chat_users(const char *name) {
    if (!repo_chat_exists(name)) {
        return make_error(2);
    }

    char **logins = NULL;
    int count = 0;

    if (repo_chat_list_users(name, &logins, &count) != 0) {
        return make_error(1);
    }

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(arr, cJSON_CreateString(logins[i]));
        free(logins[i]);
    }
    free(logins);

    return make_success(arr);
}

Response handle_add_chat_user(const char *name, Request *req) {
    if (!req->content) {
        return make_error(1);
    }

    cJSON *login_j = cJSON_GetObjectItem(req->content, "login");
    if (!cJSON_IsString(login_j) || strlen(login_j->valuestring) == 0) {
        return make_error(1);
    }

    const char *login = login_j->valuestring;

    if (!repo_chat_exists(name)) {
        return make_error(2);
    }

    if (!repo_user_exists(login)) {
        return make_error(2);
    }

    if (repo_chat_add_user(name, login) != 0) {
        return make_error(5);
    }

    return make_success(NULL);
}

Response handle_remove_chat_user(const char *name, Request *req) {
    if (!req->content) {
        return make_error(1);
    }

    cJSON *login_j = cJSON_GetObjectItem(req->content, "login");
    if (!cJSON_IsString(login_j) || strlen(login_j->valuestring) == 0) {
        return make_error(1);
    }

    const char *login = login_j->valuestring;

    if (!repo_chat_exists(name)) {
        return make_error(2);
    }

    if (repo_chat_remove_user(name, login) != 0) {
        return make_error(2);
    }

    return make_success(NULL);
}
