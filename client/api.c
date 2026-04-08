#include <api.h>
#include <model.h>
#include <state.h>
#include <connection.h>
#include <request.h>
#include <response.h>
#include <protocol.h>
#include <cJSON.h>

#include <string.h>
#include <stdio.h>

static Request make_req(RequestType type, const char *route, cJSON *content)
{
    Request req;
    req.kind    = MSG_REQUEST;
    req.type    = type;
    req.route   = (char *)route;
    req.token   = g_token[0] ? g_token : NULL;
    req.content = content;
    return req;
}

int api_get_chat_messages(const char *route, Message *msgs_out, int max, int *count_out)
{
    Request req  = make_req(GET, route, NULL);
    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content)
    {
        free_response(&res);
        return res.code;
    }

    int count    = 0;
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < max; i++)
    {
        message_from_json(cJSON_GetArrayItem(res.content, i), &msgs_out[count]);
        count++;
    }

    *count_out = count;
    free_response(&res);
    return ERR_OK;
}

int api_get_chat_list(char names_out[][MAX_ROUTE_LEN], int max, int *count_out)
{
    Request req  = make_req(GET, "/chats", NULL);
    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content)
    {
        free_response(&res);
        return res.code;
    }

    int count    = 0;
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < max; i++)
    {
        cJSON *item = cJSON_GetArrayItem(res.content, i);
        if (cJSON_IsString(item))
            strncpy(names_out[count++], item->valuestring, MAX_ROUTE_LEN - 1);
    }

    *count_out = count;
    free_response(&res);
    return ERR_OK;
}

int api_get_user_list(char names_out[][MAX_LOGIN_LEN], int max, int *count_out)
{
    Request req  = make_req(GET, "/users", NULL);
    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content)
    {
        free_response(&res);
        return res.code;
    }

    int count    = 0;
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < max; i++)
    {
        cJSON *item = cJSON_GetArrayItem(res.content, i);
        if (cJSON_IsString(item))
        {
            if (g_login[0] && strcmp(item->valuestring, g_login) == 0)
                continue;
            strncpy(names_out[count++], item->valuestring, MAX_LOGIN_LEN - 1);
        }
    }

    *count_out = count;
    free_response(&res);
    return ERR_OK;
}

int api_get_member_list(const char *chat_name, char names_out[][MAX_LOGIN_LEN], int max, int *count_out)
{
    char route[MAX_ROUTE_LEN * 2];
    snprintf(route, sizeof(route), "/chats/%s/users", chat_name);

    Request req  = make_req(GET, route, NULL);
    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content)
    {
        free_response(&res);
        return res.code;
    }

    int count    = 0;
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < max; i++)
    {
        cJSON *item = cJSON_GetArrayItem(res.content, i);
        if (cJSON_IsString(item))
            strncpy(names_out[count++], item->valuestring, MAX_LOGIN_LEN - 1);
    }

    *count_out = count;
    free_response(&res);
    return ERR_OK;
}

int api_login(const char *login, const char *password)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", login);
    cJSON_AddStringToObject(body, "password", password);

    Request req  = make_req(POST, "/login", body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    if (res.code == ERR_OK && res.content)
    {
        cJSON *token_item = cJSON_GetObjectItemCaseSensitive(res.content, "token");
        if (cJSON_IsString(token_item))
            strncpy(g_token, token_item->valuestring, MAX_TOKEN_LEN - 1);
        strncpy(g_login, login, MAX_LOGIN_LEN - 1);
    }

    int code = res.code;
    free_response(&res);
    return code;
}

int api_register(const char *login, const char *password)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", login);
    cJSON_AddStringToObject(body, "password", password);

    Request req  = make_req(POST, "/register", body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    int code = res.code;
    free_response(&res);
    return code;
}

int api_send_message(const char *route, const char *text)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "text", text);

    Request req  = make_req(POST, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    int code = res.code;
    free_response(&res);
    return code;
}

int api_create_chat(const char *name)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "name", name);

    Request req  = make_req(POST, "/chats", body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    int code = res.code;
    free_response(&res);
    return code;
}

int api_add_chat_user(const char *chat, const char *login)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", login);

    char route[MAX_ROUTE_LEN];
    snprintf(route, sizeof(route), "/chats/%s/users", chat);

    Request req  = make_req(POST, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    int code = res.code;
    free_response(&res);
    return code;
}

int api_remove_chat_user(const char *chat, const char *login)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", login);

    char route[MAX_ROUTE_LEN];
    snprintf(route, sizeof(route), "/chats/%s/users", chat);

    Request req  = make_req(DELETE, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    int code = res.code;
    free_response(&res);
    return code;
}

int api_leave_chat(const char *chat)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", "");

    char route[MAX_ROUTE_LEN];
    snprintf(route, sizeof(route), "/chats/%s/users", chat);

    Request req  = make_req(DELETE, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    int code = res.code;
    free_response(&res);
    return code;
}

int api_send_file(const char *to, const char *filepath)
{
    const char *filename = strrchr(filepath, '/');
    filename = filename ? filename + 1 : filepath;

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "filename", filename);
    cJSON_AddNumberToObject(body, "size", 0);

    char route[MAX_ROUTE_LEN];
    snprintf(route, sizeof(route), "/users/%s/files", to);

    Request req  = make_req(POST, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    int code = res.code;
    free_response(&res);
    return code;
}
