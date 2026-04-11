#include <response.h>
#include <socket_utils.h>

#include <stdlib.h>
#include <string.h>

int send_response(int socket_fd, Response response)
{
    cJSON *json = cJSON_CreateObject();
    if (!json)
        return -1;

    cJSON_AddStringToObject(json, "kind", "response");
    cJSON_AddNumberToObject(json, "code", response.code);

    if (response.content)
    {
        cJSON_AddItemReferenceToObject(json, "content", response.content);
    }

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!str)
        return -1;

    int result = write_message(socket_fd, str);
    free(str);
    return result;
}

int parse_response(char *data, Response *response)
{
    cJSON *json = cJSON_Parse(data);
    if (!json)
        return -1;

    memset(response, 0, sizeof(Response));
    response->kind = MSG_RESPONSE;

    cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
    if (cJSON_IsNumber(code))
    {
        response->code = (ErrorCode)code->valueint;
    }

    cJSON *content = cJSON_GetObjectItemCaseSensitive(json, "content");
    if (content)
    {
        response->content = cJSON_Duplicate(content, 1);
    }

    cJSON_Delete(json);
    return 0;
}

void free_response(Response *response)
{
    if (!response)
        return;
    if (response->content)
    {
        cJSON_Delete(response->content);
        response->content = NULL;
    }
}

Response make_error(ErrorCode code)
{
    Response r;
    r.kind = MSG_RESPONSE;
    r.code = code;
    r.content = NULL;
    return r;
}

Response make_success(cJSON *content)
{
    Response r;
    r.kind = MSG_RESPONSE;
    r.code = ERR_OK;
    r.content = content;
    return r;
}
