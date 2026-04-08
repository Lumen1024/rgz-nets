#include <reader.h>
#include <notification.h>
#include <connection.h>
#include <response.h>
#include <socket_utils.h>
#include <protocol.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *reader_thread(void *arg)
{
    int fd = *(int *)arg;
    free(arg);

    char buffer[MSG_BUFFER_SIZE];

    while (read_message(fd, buffer, sizeof(buffer)) == 0)
    {
        cJSON *json = cJSON_Parse(buffer);
        if (!json)
            continue;

        cJSON *kind_item = cJSON_GetObjectItemCaseSensitive(json, "kind");
        if (!cJSON_IsString(kind_item))
        {
            cJSON_Delete(json);
            continue;
        }

        const char *kind = kind_item->valuestring;

        if (strcmp(kind, "response") == 0)
        {
            Response res;
            cJSON_Delete(json);
            if (parse_response(buffer, &res) == 0)
                reader_on_response(&res);
        }
        else if (strcmp(kind, "notification") == 0)
        {
            Notification notif;
            cJSON_Delete(json);
            if (parse_notification(buffer, &notif) == 0)
            {
                handle_notification(&notif);
                free_notification(&notif);
            }
        }
        else
        {
            cJSON_Delete(json);
        }
    }

    return NULL;
}
