#include <notification.h>
#include <socket_utils.h>

#include <stdlib.h>
#include <string.h>

int send_notification(int socket_fd, Notification notification) {
    cJSON *json = cJSON_CreateObject();
    if (!json) return -1;

    cJSON_AddStringToObject(json, "kind", "notification");
    cJSON_AddNumberToObject(json, "code", notification.code);

    if (notification.content) {
        cJSON_AddItemReferenceToObject(json, "content", notification.content);
    }

    char *str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!str) return -1;

    int result = write_message(socket_fd, str);
    free(str);
    return result;
}

int parse_notification(char *data, Notification *notification) {
    cJSON *json = cJSON_Parse(data);
    if (!json) return -1;

    memset(notification, 0, sizeof(Notification));
    notification->kind = MSG_NOTIFICATION;

    cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
    if (cJSON_IsNumber(code)) {
        notification->code = (NotifCode)code->valueint;
    }

    cJSON *content = cJSON_GetObjectItemCaseSensitive(json, "content");
    if (content) {
        notification->content = cJSON_Duplicate(content, 1);
    }

    cJSON_Delete(json);
    return 0;
}

void free_notification(Notification *notification) {
    if (!notification) return;
    if (notification->content) {
        cJSON_Delete(notification->content);
        notification->content = NULL;
    }
}
