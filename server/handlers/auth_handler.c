#include "auth_handler.h"

#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "../../shared/protocol.h"
#include "../../shared/response.h"
#include "../../shared/auth.h"
#include "../repositories/user_repository.h"
#include "../repositories/chat_repository.h"

Response handle_register(Request *req) {
    if (!req->content) {
        return make_error(1);
    }

    cJSON *login_j = cJSON_GetObjectItem(req->content, "login");
    cJSON *pass_j = cJSON_GetObjectItem(req->content, "password");

    if (!cJSON_IsString(login_j) || !cJSON_IsString(pass_j)) {
        return make_error(1);
    }

    const char *login = login_j->valuestring;
    const char *password = pass_j->valuestring;

    if (strlen(login) == 0 || strlen(password) == 0) {
        return make_error(1);
    }

    if (repo_user_exists(login)) {
        return make_error(5);
    }

    char *hashed = hash_password(password);
    if (!hashed) {
        return make_error(1);
    }

    int rc = repo_user_create(login, hashed);
    free(hashed);

    if (rc != 0) {
        return make_error(1);
    }

    return make_success(NULL);
}

Response handle_login(Request *req) {
    if (!req->content) {
        return make_error(1);
    }

    cJSON *login_j = cJSON_GetObjectItem(req->content, "login");
    cJSON *pass_j = cJSON_GetObjectItem(req->content, "password");

    if (!cJSON_IsString(login_j) || !cJSON_IsString(pass_j)) {
        return make_error(1);
    }

    const char *login = login_j->valuestring;
    const char *password = pass_j->valuestring;

    if (!repo_user_exists(login)) {
        return make_error(2);
    }

    char hash_out[256];
    if (repo_user_get_hash(login, hash_out) != 0) {
        return make_error(1);
    }

    if (!verify_password(password, hash_out)) {
        return make_error(3);
    }

    char *token = generate_token(login);
    if (!token) {
        return make_error(1);
    }

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "token", token);
    free(token);

    return make_success(body);
}

Response handle_info(Request *req) {
    (void)req;

    char **logins = NULL;
    int users_count = 0;
    repo_user_list(&logins, &users_count);

    if (logins) {
        for (int i = 0; i < users_count; i++) {
            free(logins[i]);
        }
        free(logins);
    }

    // Iterate through users to find all unique chats
    char **all_logins = NULL;
    int total_users = 0;
    repo_user_list(&all_logins, &total_users);

    int chat_count = 0;
    char **seen_chats = NULL;
    int seen_count = 0;

    for (int u = 0; u < total_users; u++) {
        char **chats = NULL;
        int cnt = 0;
        repo_chat_list_for_user(all_logins[u], &chats, &cnt);
        for (int c = 0; c < cnt; c++) {
            // Check if already seen
            int found = 0;
            for (int s = 0; s < seen_count; s++) {
                if (strcmp(seen_chats[s], chats[c]) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                seen_chats = realloc(seen_chats, sizeof(char *) * (seen_count + 1));
                seen_chats[seen_count] = strdup(chats[c]);
                seen_count++;
            }
            free(chats[c]);
        }
        free(chats);
        free(all_logins[u]);
    }
    free(all_logins);

    chat_count = seen_count;
    for (int i = 0; i < seen_count; i++) {
        free(seen_chats[i]);
    }
    free(seen_chats);

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "name", "Chat Server");
    cJSON_AddNumberToObject(body, "users_count", users_count);
    cJSON_AddNumberToObject(body, "chat_count", chat_count);

    return make_success(body);
}
