#include <protocol.h>

#include <string.h>

int parse_message_kind(const char *buf, MessageKind *kind_out)
{
    cJSON *json = cJSON_Parse(buf);
    if (!json)
        return -1;

    cJSON *kind_item = cJSON_GetObjectItemCaseSensitive(json, "kind");
    if (!cJSON_IsString(kind_item))
    {
        cJSON_Delete(json);
        return -1;
    }

    const char *kind = kind_item->valuestring;
    int result = 0;

    if (strcmp(kind, "response") == 0)
        *kind_out = MSG_RESPONSE;
    else if (strcmp(kind, "notification") == 0)
        *kind_out = MSG_NOTIFICATION;
    else if (strcmp(kind, "request") == 0)
        *kind_out = MSG_REQUEST;
    else
        result = -1;

    cJSON_Delete(json);
    return result;
}
