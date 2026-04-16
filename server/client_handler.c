#include <client_handler.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <protocol.h>
#include <request.h>
#include <response.h>
#include <socket_utils.h>
#include <auth.h>

#include <auth_handler.h>
#include <chat_handler.h>
#include <message_handler.h>
#include <file_handler.h>
#include <user_repository.h>
#include <notify/notify.h>

// Extract segment from route at position idx (0-based), returns 0 on success
static int route_segment(const char *route, int idx, char *out, size_t out_size)
{
    const char *p = route;
    if (*p == '/')
        p++;

    for (int i = 0; i < idx; i++)
    {
        p = strchr(p, '/');
        if (!p)
            return -1;
        p++;
    }

    const char *end = strchr(p, '/');
    size_t len = end ? (size_t)(end - p) : strlen(p);
    if (len == 0 || len >= out_size)
        return -1;

    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

// Returns number of '/' separated segments in route
static int route_depth(const char *route)
{
    int depth = 0;
    const char *p = route;
    if (*p == '/')
        p++;
    if (*p == '\0')
        return 0;
    depth = 1;
    while ((p = strchr(p, '/')) != NULL)
    {
        depth++;
        p++;
    }
    return depth;
}

static Response dispatch(Request *req, const char *login)
{
    const char *route = req->route ? req->route : "";
    RequestType type = req->type;

    char seg0[MAX_ROUTE_LEN] = {0};
    char seg1[MAX_ROUTE_LEN] = {0};
    char seg2[MAX_ROUTE_LEN] = {0};
    char seg3[MAX_ROUTE_LEN] = {0};

    int depth = route_depth(route);
    route_segment(route, 0, seg0, sizeof(seg0));
    route_segment(route, 1, seg1, sizeof(seg1));
    route_segment(route, 2, seg2, sizeof(seg2));
    route_segment(route, 3, seg3, sizeof(seg3));

    // Routes that don't require auth
    if (strcmp(seg0, "info") == 0 && depth == 1 && type == GET)
        return handle_info(req);
    if (strcmp(seg0, "register") == 0 && depth == 1 && type == POST)
        return handle_register(req);
    if (strcmp(seg0, "login") == 0 && depth == 1 && type == POST)
        return handle_login(req);

    // All routes below require auth
    if (login == NULL || login[0] == '\0')
        return make_error(ERR_UNAUTHORIZED);

    // GET /users
    if (strcmp(seg0, "users") == 0 && depth == 1 && type == GET)
    {
        char **logins = NULL;
        int count = 0;
        if (repo_user_list(&logins, &count) != 0)
            return make_error(ERR_INTERNAL);
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++)
        {
            cJSON_AddItemToArray(arr, cJSON_CreateString(logins[i]));
            free(logins[i]);
        }
        free(logins);
        return make_success(arr);
    }

    // /users/{login}/messages
    if (strcmp(seg0, "users") == 0 && depth == 3 && strcmp(seg2, "messages") == 0)
    {
        if (type == GET)
            return handle_get_private_messages(login, seg1);
        if (type == POST)
            return handle_post_private_message(seg1, req, login);
    }

    // /users/{login}/files
    if (strcmp(seg0, "users") == 0 && depth == 3 && strcmp(seg2, "files") == 0 && type == POST)
        return handle_file_request(seg1, req, login);

    // /users/{login}/files/{id}/approve  or  /users/{login}/files/{id}/decline
    if (strcmp(seg0, "users") == 0 && depth == 5 && strcmp(seg2, "files") == 0)
    {
        char seg4[MAX_ROUTE_LEN] = {0};
        route_segment(route, 4, seg4, sizeof(seg4));
        if (strcmp(seg4, "approve") == 0 && type == POST)
            return handle_file_approve(seg1, seg3);
        if (strcmp(seg4, "decline") == 0 && type == POST)
            return handle_file_decline(seg1, seg3);
    }

    // GET/POST /chats
    if (strcmp(seg0, "chats") == 0 && depth == 1)
    {
        if (type == GET)
            return handle_get_chats(login);
        if (type == POST)
            return handle_create_chat(req, login);
    }

    // /chats/{name}
    if (strcmp(seg0, "chats") == 0 && depth == 2)
    {
        if (type == DELETE)
            return handle_delete_chat(seg1, login);
    }

    // /chats/{name}/host
    if (strcmp(seg0, "chats") == 0 && depth == 3 && strcmp(seg2, "host") == 0 && type == GET)
        return handle_get_chat_host(seg1);

    // /chats/{name}/users
    if (strcmp(seg0, "chats") == 0 && depth == 3 && strcmp(seg2, "users") == 0)
    {
        if (type == GET)
            return handle_get_chat_users(seg1);
        if (type == POST)
            return handle_add_chat_user(seg1, req);
        if (type == DELETE)
            return handle_remove_chat_user(seg1, req);
    }

    // /chats/{name}/messages
    if (strcmp(seg0, "chats") == 0 && depth == 3 && strcmp(seg2, "messages") == 0)
    {
        if (type == GET)
            return handle_get_chat_messages(seg1);
        if (type == POST)
            return handle_post_chat_message(seg1, req, login);
    }

    return make_error(ERR_NOT_FOUND);
}

void handle_client(int socket_fd)
{
    char buf[MSG_BUFFER_SIZE];
    char login[MAX_LOGIN_LEN] = {0};

    while (1)
    {
        // block
        if (read_message(socket_fd, buf, sizeof(buf)) != 0)
            break;

        Request req;
        if (parse_request(buf, &req) != 0)
        {
            Response err = make_error(ERR_BAD_REQUEST);
            send_response(socket_fd, err);
            continue;
        }

        // extract login if token valid
        if (req.token && req.token[0] != '\0' && login[0] == '\0')
        {
            char req_login[MAX_LOGIN_LEN] = {0};
            if (validate_token(req.token, req_login) == 0)
            {
                strncpy(login, req_login, MAX_LOGIN_LEN - 1);
                notify_register(login);
            }
        }

        Response resp = dispatch(&req, login[0] ? login : NULL);

        if (login[0])
            printf("[%s] %s %s -> %d\n", login, request_type_str(req.type), req.route ? req.route : "/", resp.code);
        else
            printf("[anonymous] %s %s -> %d\n", request_type_str(req.type), req.route ? req.route : "/", resp.code);
        fflush(stdout);

        send_response(socket_fd, resp);

        free_request(&req);
        if (resp.content)
            cJSON_Delete(resp.content);
    }

    notify_unregister(socket_fd);
    close(socket_fd);
}
