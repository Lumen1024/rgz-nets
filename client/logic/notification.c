#include <notification.h>
#include <api/model.h>
#include <ui.h>
#include <protocol.h>

#include <string.h>
#include <stdio.h>

void handle_notification(Notification *notif)
{
    if (notif->code == NOTIF_NEW_MESSAGE)
    {
        cJSON *content = notif->content;
        if (!content)
            return;

        cJSON *login_item = cJSON_GetObjectItemCaseSensitive(content, "login");
        cJSON *chat_item = cJSON_GetObjectItemCaseSensitive(content, "chat");
        cJSON *to_item = cJSON_GetObjectItemCaseSensitive(content, "to");

        char msg_route[MAX_ROUTE_LEN] = {0};
        if (cJSON_IsString(chat_item))
        {
            snprintf(msg_route, sizeof(msg_route), "/chats/%s/messages",
                     chat_item->valuestring);
        }
        else if (cJSON_IsString(to_item) && cJSON_IsString(login_item))
        {
            const char *current = ui_get_current_chat();
            const char *sender = login_item->valuestring;
            const char *to = to_item->valuestring;
            if (current)
            {
                char route_a[MAX_ROUTE_LEN], route_b[MAX_ROUTE_LEN];
                snprintf(route_a, sizeof(route_a), "/users/%s/messages", sender);
                snprintf(route_b, sizeof(route_b), "/users/%s/messages", to);
                if (strcmp(current, route_a) == 0 || strcmp(current, route_b) == 0)
                    strncpy(msg_route, current, sizeof(msg_route) - 1);
            }
        }

        const char *current_chat = ui_get_current_chat();
        int is_current = current_chat && msg_route[0] &&
                         strcmp(current_chat, msg_route) == 0;

        Message msg;
        message_from_json(content, &msg);

        if (is_current)
        {
            ui_append_message(&msg);
        }
        else if (msg.login[0] && msg.text[0])
        {
            char notif_text[MAX_ROUTE_LEN + MAX_LOGIN_LEN + MAX_TEXT_LEN];
            snprintf(notif_text, sizeof(notif_text), "[%s] %s: %s",
                     msg_route[0] ? msg_route : "?", msg.login, msg.text);
            ui_notify(notif_text);
        }
    }
    else if (notif->code == NOTIF_FILE_REQUEST)
    {
        cJSON *content = notif->content;
        if (!content)
            return;

        cJSON *from_item = cJSON_GetObjectItemCaseSensitive(content, "from");
        cJSON *name_item = cJSON_GetObjectItemCaseSensitive(content, "filename");
        cJSON *id_item = cJSON_GetObjectItemCaseSensitive(content, "file_id");

        char msg[256];
        snprintf(msg, sizeof(msg), "File request from %s: %s (id=%s)",
                 cJSON_IsString(from_item) ? from_item->valuestring : "?",
                 cJSON_IsString(name_item) ? name_item->valuestring : "?",
                 cJSON_IsString(id_item) ? id_item->valuestring : "?");
        ui_notify(msg);
    }
}
