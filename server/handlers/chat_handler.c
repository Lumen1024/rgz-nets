#include <chat_handler.h>

#include <stdlib.h>
#include <string.h>
#include <cJSON.h>
#include <protocol.h>
#include <response.h>
#include <chat_repository.h>
#include <user_repository.h>

Response handle_get_chats(const char *login) {
    char **names = NULL;
    int count = 0;

    if (repo_chat_list_for_user(login, &names, &count) != 0) {
        return make_error(ERR_INTERNAL);
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
        return make_error(ERR_BAD_REQUEST);
    }

    cJSON *name_j = cJSON_GetObjectItem(req->content, "name");
    if (!cJSON_IsString(name_j) || strlen(name_j->valuestring) == 0) {
        return make_error(ERR_BAD_REQUEST);
    }

    const char *name = name_j->valuestring;

    if (repo_chat_exists(name)) {
        return make_error(ERR_CONFLICT);
    }

    if (repo_chat_create(name, login) != 0) {
        return make_error(ERR_INTERNAL);
    }

    // Add the creator as the first member
    repo_chat_add_user(name, login);

    return make_success(NULL);
}

Response handle_delete_chat(const char *name, const char *login) {
    if (!repo_chat_exists(name)) {
        return make_error(ERR_NOT_FOUND);
    }

    char host[256];
    if (repo_chat_get_host(name, host) != 0) {
        return make_error(ERR_INTERNAL);
    }

    if (strcmp(host, login) != 0) {
        return make_error(ERR_FORBIDDEN);
    }

    if (repo_chat_delete(name) != 0) {
        return make_error(ERR_INTERNAL);
    }

    return make_success(NULL);
}

Response handle_get_chat_host(const char *name) {
    if (!repo_chat_exists(name)) {
        return make_error(ERR_NOT_FOUND);
    }

    char host[256];
    if (repo_chat_get_host(name, host) != 0) {
        return make_error(ERR_INTERNAL);
    }

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", host);

    return make_success(body);
}

Response handle_get_chat_users(const char *name) {
    if (!repo_chat_exists(name)) {
        return make_error(ERR_NOT_FOUND);
    }

    char **logins = NULL;
    int count = 0;

    if (repo_chat_list_users(name, &logins, &count) != 0) {
        return make_error(ERR_INTERNAL);
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
        return make_error(ERR_BAD_REQUEST);
    }

    cJSON *login_j = cJSON_GetObjectItem(req->content, "login");
    if (!cJSON_IsString(login_j) || strlen(login_j->valuestring) == 0) {
        return make_error(ERR_BAD_REQUEST);
    }

    const char *login = login_j->valuestring;

    if (!repo_chat_exists(name)) {
        return make_error(ERR_NOT_FOUND);
    }

    if (!repo_user_exists(login)) {
        return make_error(ERR_NOT_FOUND);
    }

    if (repo_chat_add_user(name, login) != 0) {
        return make_error(ERR_CONFLICT);
    }

    return make_success(NULL);
}

Response handle_remove_chat_user(const char *name, Request *req) {
    if (!req->content) {
        return make_error(ERR_BAD_REQUEST);
    }

    cJSON *login_j = cJSON_GetObjectItem(req->content, "login");
    if (!cJSON_IsString(login_j) || strlen(login_j->valuestring) == 0) {
        return make_error(ERR_BAD_REQUEST);
    }

    const char *login = login_j->valuestring;

    if (!repo_chat_exists(name)) {
        return make_error(ERR_NOT_FOUND);
    }

    if (repo_chat_remove_user(name, login) != 0) {
        return make_error(ERR_NOT_FOUND);
    }

    return make_success(NULL);
}
