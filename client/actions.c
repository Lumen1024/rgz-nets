#include <actions.h>
#include <connection.h>
#include <ui.h>
#include <request.h>
#include <response.h>
#include <protocol.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>

// Token and login stored after successful login
static char g_token[MAX_TOKEN_LEN]    = {0};
static char g_login[MAX_LOGIN_LEN]    = {0};

const char *actions_get_token(void) {
    return g_token[0] ? g_token : NULL;
}

const char *actions_get_login(void) {
    return g_login[0] ? g_login : NULL;
}

static Request make_req(RequestType type, const char *route, cJSON *content) {
    Request req;
    req.kind    = MSG_REQUEST;
    req.type    = type;
    req.route   = (char *)route;
    req.token   = g_token[0] ? g_token : NULL;
    req.content = content;
    return req;
}

void *action_login(void *args) {
    AuthArgs *a = (AuthArgs *)args;

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login",    a->login);
    cJSON_AddStringToObject(body, "password", a->password);

    Request req = make_req(POST, "/login", body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    if (res.code == ERR_OK && res.content) {
        cJSON *token_item = cJSON_GetObjectItemCaseSensitive(res.content, "token");
        if (cJSON_IsString(token_item)) {
            strncpy(g_token, token_item->valuestring, MAX_TOKEN_LEN - 1);
        }
        strncpy(g_login, a->login, MAX_LOGIN_LEN - 1);
        free_response(&res);
        return (void *)0;
    }

    free_response(&res);
    return (void *)(intptr_t)res.code;
}

void *action_register(void *args) {
    AuthArgs *a = (AuthArgs *)args;

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login",    a->login);
    cJSON_AddStringToObject(body, "password", a->password);

    Request req = make_req(POST, "/register", body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    free_response(&res);
    return (void *)(intptr_t)res.code;
}

void *action_send_message(void *args) {
    SendMessageArgs *a = (SendMessageArgs *)args;

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "text", a->text);

    // Build route: /chats/{name}/messages or /users/{login}/messages
    // For simplicity the caller puts the full route in a->chat
    char route[MAX_ROUTE_LEN];
    snprintf(route, sizeof(route), "%s", a->chat);

    Request req = make_req(POST, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    free_response(&res);
    return (void *)(intptr_t)res.code;
}

void *action_create_chat(void *args) {
    CreateChatArgs *a = (CreateChatArgs *)args;

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "name", a->name);

    Request req = make_req(POST, "/chats", body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    free_response(&res);
    return (void *)(intptr_t)res.code;
}

void *action_add_chat_user(void *args) {
    ChatUserArgs *a = (ChatUserArgs *)args;

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", a->login);

    char route[MAX_ROUTE_LEN];
    snprintf(route, sizeof(route), "/chats/%s/users", a->chat);

    Request req = make_req(POST, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    free_response(&res);
    return (void *)(intptr_t)res.code;
}

void *action_remove_chat_user(void *args) {
    ChatUserArgs *a = (ChatUserArgs *)args;

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", a->login);

    char route[MAX_ROUTE_LEN];
    snprintf(route, sizeof(route), "/chats/%s/users", a->chat);

    Request req = make_req(DELETE, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    free_response(&res);
    return (void *)(intptr_t)res.code;
}

void *action_leave_chat(void *args) {
    LeaveChatArgs *a = (LeaveChatArgs *)args;

    // Use empty login — server will use token to identify the user
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", "");

    char route[MAX_ROUTE_LEN];
    snprintf(route, sizeof(route), "/chats/%s/users", a->chat);

    Request req = make_req(DELETE, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    free_response(&res);
    return (void *)(intptr_t)res.code;
}

void *action_send_file(void *args) {
    SendFileArgs *a = (SendFileArgs *)args;

    // Extract filename from path
    const char *filename = strrchr(a->filepath, '/');
    filename = filename ? filename + 1 : a->filepath;

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "filename", filename);
    cJSON_AddNumberToObject(body, "size", 0); // caller can fill real size

    char route[MAX_ROUTE_LEN];
    snprintf(route, sizeof(route), "/users/%s/files", a->to);

    Request req = make_req(POST, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    free_response(&res);
    return (void *)(intptr_t)res.code;
}
