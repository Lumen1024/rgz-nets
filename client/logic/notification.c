#include <notification.h>
#include <api/model.h>
#include <api.h>
#include <ui.h>
#include <protocol.h>

#include <string.h>
#include <stdio.h>

void handle_notification(Notification *notif)
{
    if (notif->code == NOTIF_CHAT_MEMBERS_CHANGED)
    {
        cJSON *content = notif->content;
        if (!content)
            return;

        cJSON *chat_item = cJSON_GetObjectItemCaseSensitive(content, "chat");
        if (!cJSON_IsString(chat_item))
            return;

        const char *chat_name = chat_item->valuestring;
        const char *current = ui_get_current_chat();

        // Update member list panel if the affected chat is currently open
        char current_chat_name[MAX_ROUTE_LEN] = {0};
        if (current && strncmp(current, "/chats/", 7) == 0)
        {
            const char *p = current + 7;
            const char *slash = strchr(p, '/');
            int len = slash ? (int)(slash - p) : (int)strlen(p);
            if (len > 0 && len < (int)sizeof(current_chat_name))
            {
                strncpy(current_chat_name, p, len);
                current_chat_name[len] = '\0';
            }
        }

        if (current_chat_name[0] && strcmp(current_chat_name, chat_name) == 0)
        {
            char names[MAX_MEMBERS][MAX_LOGIN_LEN];
            int count = 0;
            if (api_get_member_list(chat_name, names, MAX_MEMBERS, &count) == ERR_OK)
            {
                char *ptrs[MAX_MEMBERS];
                for (int i = 0; i < count; i++)
                    ptrs[i] = names[i];
                ui_set_member_list(ptrs, count);
            }
        }
        return;
    }

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
            const char *sender = login_item->valuestring;
            snprintf(msg_route, sizeof(msg_route), "/users/%s/messages", sender);
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
            if (cJSON_IsString(chat_item))
            {
                snprintf(notif_text, sizeof(notif_text), "[%s] %s: %s",
                         chat_item->valuestring, msg.login, msg.text);
            }
            else
            {
                snprintf(notif_text, sizeof(notif_text), "%s: %s",
                         msg.login, msg.text);
            }
            ui_notify(notif_text);
        }
    }
}
