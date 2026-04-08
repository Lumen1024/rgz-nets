#include <model.h>

#include <string.h>

void message_from_json(cJSON *item, Message *out)
{
    memset(out, 0, sizeof(Message));
    cJSON *l  = cJSON_GetObjectItemCaseSensitive(item, "login");
    cJSON *t  = cJSON_GetObjectItemCaseSensitive(item, "text");
    cJSON *ts = cJSON_GetObjectItemCaseSensitive(item, "timestamp");
    if (cJSON_IsString(l))
        strncpy(out->login, l->valuestring, MAX_LOGIN_LEN - 1);
    if (cJSON_IsString(t))
        strncpy(out->text, t->valuestring, MAX_TEXT_LEN - 1);
    if (cJSON_IsString(ts))
        strncpy(out->timestamp, ts->valuestring, MAX_TIMESTAMP_LEN - 1);
}
