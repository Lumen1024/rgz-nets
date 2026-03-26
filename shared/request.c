#include "request.h"
#include "socket_utils.h"

#include <stdlib.h>
#include <string.h>

static const char *request_type_to_string(RequestType type) {
    switch (type) {
        case GET:    return "GET";
        case POST:   return "POST";
        case DELETE:  return "DELETE";
        default:     return "GET";
    }
}

static RequestType string_to_request_type(const char *str) {
    if (strcmp(str, "POST") == 0)   return POST;
    if (strcmp(str, "DELETE") == 0) return DELETE;
    return GET;
}

int send_request(int socket_fd, Request request) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;

    cJSON_AddStringToObject(json, "kind", "request");
    cJSON_AddStringToObject(json, "route", request.route ? request.route : "");
    cJSON_AddStringToObject(json, "type", request_type_to_string(request.type));

    if (request.token) {
        cJSON *headers = cJSON_CreateObject();
        cJSON_AddStringToObject(headers, "Authorization", request.token);
        cJSON_AddItemToObject(json, "headers", headers);
    }

    if (request.content) {
        cJSON_AddItemReferenceToObject(json, "content", request.content);
    }

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!str) return -1;

    int result = write_message(socket_fd, str);
    free(str);
    return result;
}

int parse_request(char *data, Request *request) {
    cJSON *json = cJSON_Parse(data);
    if (!json) return -1;

    memset(request, 0, sizeof(Request));
    request->kind = MSG_REQUEST;

    cJSON *route = cJSON_GetObjectItemCaseSensitive(json, "route");
    if (cJSON_IsString(route)) {
        request->route = strdup(route->valuestring);
    }

    cJSON *type = cJSON_GetObjectItemCaseSensitive(json, "type");
    if (cJSON_IsString(type)) {
        request->type = string_to_request_type(type->valuestring);
    }

    cJSON *headers = cJSON_GetObjectItemCaseSensitive(json, "headers");
    if (cJSON_IsObject(headers)) {
        cJSON *auth = cJSON_GetObjectItemCaseSensitive(headers, "Authorization");
        if (cJSON_IsString(auth)) {
            request->token = strdup(auth->valuestring);
        }
    }

    cJSON *content = cJSON_GetObjectItemCaseSensitive(json, "content");
    if (content) {
        request->content = cJSON_Duplicate(content, 1);
    }

    cJSON_Delete(json);
    return 0;
}

void free_request(Request *request) {
    if (!request) return;
    free(request->route);
    request->route = NULL;
    free(request->token);
    request->token = NULL;
    if (request->content) {
        cJSON_Delete(request->content);
        request->content = NULL;
    }
}
