#include <reader.h>
#include <connection.h>
#include <ui.h>
#include <response.h>
#include <notification.h>
#include <socket_utils.h>
#include <protocol.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *reader_thread(void *arg) {
    int fd = *(int *)arg;
    free(arg);

    char buffer[MSG_BUFFER_SIZE];

    while (read_message(fd, buffer, sizeof(buffer)) == 0) {
        // Peek at "kind" field to route the message
        cJSON *json = cJSON_Parse(buffer);
        if (!json) continue;

        cJSON *kind_item = cJSON_GetObjectItemCaseSensitive(json, "kind");
        if (!cJSON_IsString(kind_item)) {
            cJSON_Delete(json);
            continue;
        }

        const char *kind = kind_item->valuestring;

        if (strcmp(kind, "response") == 0) {
            Response res;
            cJSON_Delete(json);
            if (parse_response(buffer, &res) == 0) {
                reader_on_response(&res);
                // Ownership transferred to send_and_wait caller; do not free here
            }
        } else if (strcmp(kind, "notification") == 0) {
            Notification notif;
            cJSON_Delete(json);
            if (parse_notification(buffer, &notif) == 0) {
                reader_on_notification(&notif);
                free_notification(&notif);
            }
        } else {
            cJSON_Delete(json);
        }
    }

    return NULL;
}

void reader_on_notification(Notification *notif) {
    if (notif->code == NOTIF_NEW_MESSAGE) {
        cJSON *content = notif->content;
        if (!content) return;

        cJSON *login_item     = cJSON_GetObjectItemCaseSensitive(content, "login");
        cJSON *text_item      = cJSON_GetObjectItemCaseSensitive(content, "text");
        cJSON *timestamp_item = cJSON_GetObjectItemCaseSensitive(content, "timestamp");
        cJSON *chat_item      = cJSON_GetObjectItemCaseSensitive(content, "chat");
        cJSON *to_item        = cJSON_GetObjectItemCaseSensitive(content, "to");

        // Build the route this message belongs to
        char msg_route[MAX_ROUTE_LEN] = {0};
        if (cJSON_IsString(chat_item)) {
            snprintf(msg_route, sizeof(msg_route), "/chats/%s/messages", chat_item->valuestring);
        } else if (cJSON_IsString(to_item) && cJSON_IsString(login_item)) {
            // Private message: route is either /users/{sender}/messages or /users/{to}/messages
            // from our perspective the open route can be either side of the dialog
            const char *current = ui_get_current_chat();
            const char *sender  = login_item->valuestring;
            const char *to      = to_item->valuestring;
            if (current) {
                char route_a[MAX_ROUTE_LEN], route_b[MAX_ROUTE_LEN];
                snprintf(route_a, sizeof(route_a), "/users/%s/messages", sender);
                snprintf(route_b, sizeof(route_b), "/users/%s/messages", to);
                if (strcmp(current, route_a) == 0 || strcmp(current, route_b) == 0) {
                    strncpy(msg_route, current, sizeof(msg_route) - 1);
                }
            }
        }

        const char *current_chat = ui_get_current_chat();
        int is_current = current_chat && msg_route[0] && strcmp(current_chat, msg_route) == 0;

        Message msg;
        memset(&msg, 0, sizeof(msg));

        if (cJSON_IsString(login_item))
            strncpy(msg.login, login_item->valuestring, MAX_LOGIN_LEN - 1);
        if (cJSON_IsString(text_item))
            strncpy(msg.text, text_item->valuestring, MAX_TEXT_LEN - 1);
        if (cJSON_IsString(timestamp_item))
            strncpy(msg.timestamp, timestamp_item->valuestring, MAX_TIMESTAMP_LEN - 1);

        if (is_current) {
            ui_append_message(&msg);
        } else if (msg.login[0] && msg.text[0]) {
            // Message in a different chat — show as notification
            char notif_text[256];
            snprintf(notif_text, sizeof(notif_text), "[%s] %s: %s",
                msg_route[0] ? msg_route : "?", msg.login, msg.text);
            ui_notify(notif_text);
        }
    } else if (notif->code == NOTIF_FILE_REQUEST) {
        cJSON *content = notif->content;
        if (!content) return;

        cJSON *from_item = cJSON_GetObjectItemCaseSensitive(content, "from");
        cJSON *name_item = cJSON_GetObjectItemCaseSensitive(content, "filename");
        cJSON *id_item   = cJSON_GetObjectItemCaseSensitive(content, "file_id");

        char msg[256];
        snprintf(msg, sizeof(msg), "File request from %s: %s (id=%s)",
            cJSON_IsString(from_item) ? from_item->valuestring : "?",
            cJSON_IsString(name_item) ? name_item->valuestring : "?",
            cJSON_IsString(id_item)   ? id_item->valuestring   : "?");
        ui_notify(msg);
    }
}
