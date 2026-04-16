#include <api.h>
#include <api/model.h>
#include <state.h>
#include <request.h>
#include <response.h>
#include <notification.h>
#include <socket_utils.h>
#include <protocol.h>
#include <cJSON.h>
#include <logic/notification.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

static int g_socket_fd = -1;

static pthread_mutex_t g_resp_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_resp_cond  = PTHREAD_COND_INITIALIZER;
static Response        g_pending_response;
static int             g_response_ready = 0;

static void reader_on_response(Response *res)
{
    pthread_mutex_lock(&g_resp_mutex);
    g_pending_response = *res;
    g_response_ready   = 1;
    pthread_cond_signal(&g_resp_cond);
    pthread_mutex_unlock(&g_resp_mutex);
}

static void *reader_thread(void *arg)
{
    int fd = *(int *)arg;
    free(arg);

    char buffer[MSG_BUFFER_SIZE];

    while (read_message(fd, buffer, sizeof(buffer)) == 0)
    {
        MessageKind kind;
        if (parse_message_kind(buffer, &kind) != 0)
            continue;

        if (kind == MSG_RESPONSE)
        {
            Response res;
            if (parse_response(buffer, &res) == 0)
                reader_on_response(&res);
        }
        else if (kind == MSG_NOTIFICATION)
        {
            Notification notif;
            if (parse_notification(buffer, &notif) == 0)
            {
                handle_notification(&notif);
                free_notification(&notif);
            }
        }
    }

    return NULL;
}

int connect_to_server(const char *host, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0)
    {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }

    g_socket_fd = fd;

    pthread_t tid;
    int *fd_ptr = malloc(sizeof(int));
    *fd_ptr = fd;
    pthread_create(&tid, NULL, reader_thread, fd_ptr);
    pthread_detach(tid);

    return fd;
}

static Response send_and_wait(Request req)
{
    pthread_mutex_lock(&g_resp_mutex);
    g_response_ready = 0;
    pthread_mutex_unlock(&g_resp_mutex);

    if (send_request(g_socket_fd, req) < 0)
    {
        Response err = {.kind = MSG_RESPONSE, .code = ERR_INTERNAL, .content = NULL};
        return err;
    }

    pthread_mutex_lock(&g_resp_mutex);
    while (!g_response_ready)
        pthread_cond_wait(&g_resp_cond, &g_resp_mutex);
    Response res = g_pending_response;
    pthread_mutex_unlock(&g_resp_mutex);

    return res;
}

static Request make_req(RequestType type, const char *route, cJSON *content)
{
    Request req;
    req.kind = MSG_REQUEST;
    req.type = type;
    req.route = (char *)route;
    req.token = g_token[0] ? g_token : NULL;
    req.content = content;
    return req;
}

int api_get_chat_messages(const char *route, Message *msgs_out, int max, int *count_out)
{
    Request req = make_req(GET, route, NULL);
    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content)
    {
        free_response(&res);
        return res.code;
    }

    int count = 0;
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
    Request req = make_req(GET, "/chats", NULL);
    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content)
    {
        free_response(&res);
        return res.code;
    }

    int count = 0;
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
    Request req = make_req(GET, "/users", NULL);
    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content)
    {
        free_response(&res);
        return res.code;
    }

    int count = 0;
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

    Request req = make_req(GET, route, NULL);
    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content)
    {
        free_response(&res);
        return res.code;
    }

    int count = 0;
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

const char *api_get_login()
{
    return g_login[0] ? g_login : NULL;
}

int api_login(const char *login, const char *password)
{
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "login", login);
    cJSON_AddStringToObject(body, "password", password);

    Request req = make_req(POST, "/login", body);
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

    Request req = make_req(POST, "/register", body);
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

    Request req = make_req(POST, route, body);
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

    Request req = make_req(POST, "/chats", body);
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

    Request req = make_req(POST, route, body);
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

    Request req = make_req(DELETE, route, body);
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

    Request req = make_req(DELETE, route, body);
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

    Request req = make_req(POST, route, body);
    Response res = send_and_wait(req);
    cJSON_Delete(body);

    int code = res.code;
    free_response(&res);
    return code;
}
