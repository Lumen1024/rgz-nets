#include "data.h"
#include "state.h"

#include <ui.h>
#include <actions.h>
#include <connection.h>
#include <request.h>
#include <response.h>
#include <protocol.h>
#include <cJSON.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ─── Load messages for a chat/dialog route ───────────────────────────────────

void load_chat_messages(const char *route) {
    Request req;
    req.kind    = MSG_REQUEST;
    req.type    = GET;
    req.route   = (char *)route;
    req.token   = (char *)actions_get_token();
    req.content = NULL;

    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content) {
        free_response(&res);
        return;
    }

    Message msgs[MAX_MESSAGES];
    int count = 0;
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < MAX_MESSAGES; i++) {
        cJSON *item = cJSON_GetArrayItem(res.content, i);
        cJSON *l    = cJSON_GetObjectItemCaseSensitive(item, "login");
        cJSON *t    = cJSON_GetObjectItemCaseSensitive(item, "text");
        cJSON *ts   = cJSON_GetObjectItemCaseSensitive(item, "timestamp");

        memset(&msgs[count], 0, sizeof(Message));
        if (cJSON_IsString(l))  strncpy(msgs[count].login,     l->valuestring,  MAX_LOGIN_LEN     - 1);
        if (cJSON_IsString(t))  strncpy(msgs[count].text,      t->valuestring,  MAX_TEXT_LEN      - 1);
        if (cJSON_IsString(ts)) strncpy(msgs[count].timestamp, ts->valuestring, MAX_TIMESTAMP_LEN - 1);
        count++;
    }

    free_response(&res);
    ui_set_chat(route, msgs, count);
}

// ─── Load list of group chats ─────────────────────────────────────────────────

void load_chat_list(void) {
    Request req;
    req.kind    = MSG_REQUEST;
    req.type    = GET;
    req.route   = "/chats";
    req.token   = (char *)actions_get_token();
    req.content = NULL;

    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content) {
        free_response(&res);
        return;
    }

    char *names[MAX_CHATS];
    int count = 0;
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < MAX_CHATS; i++) {
        cJSON *item = cJSON_GetArrayItem(res.content, i);
        if (cJSON_IsString(item)) {
            names[count++] = item->valuestring;
        }
    }

    ui_set_chat_list(names, count);
    free_response(&res);
}

// ─── Load list of all users ───────────────────────────────────────────────────

void load_user_list(void) {
    Request req;
    req.kind    = MSG_REQUEST;
    req.type    = GET;
    req.route   = "/users";
    req.token   = (char *)actions_get_token();
    req.content = NULL;

    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content) {
        free_response(&res);
        return;
    }

    char *names[MAX_USERS];
    int  has_msg[MAX_USERS];
    int count = 0;
    const char *my_login = actions_get_login();
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < MAX_USERS; i++) {
        cJSON *item = cJSON_GetArrayItem(res.content, i);
        if (cJSON_IsString(item)) {
            if (my_login && strcmp(item->valuestring, my_login) == 0) continue;
            names[count]   = item->valuestring;
            has_msg[count] = 0;
            count++;
        }
    }

    ui_set_user_list(names, has_msg, count);
    free_response(&res);
}

// ─── Open selected item from the list panel ───────────────────────────────────

void open_selected_item(void) {
    int count = (g_list_mode == LIST_MODE_CHATS) ? g_chat_count : g_user_count;
    if (count == 0) return;

    if (g_list_mode == LIST_MODE_CHATS) {
        char route[CHAT_ROUTE_LEN];
        snprintf(route, sizeof(route), "/chats/%s/messages",
            g_chat_names[g_list_selected]);
        g_active = PANEL_NONE;
        g_focus  = PANEL_CHAT;
        load_chat_messages(route);
    } else {
        const char *my_login = actions_get_login();
        if (my_login && strcmp(g_user_names[g_list_selected], my_login) == 0) {
            ui_sys("Cannot open dialog with yourself");
            return;
        }
        char route[MAX_ROUTE_LEN];
        snprintf(route, sizeof(route), "/users/%s/messages",
            g_user_names[g_list_selected]);
        g_active = PANEL_NONE;
        g_focus  = PANEL_CHAT;
        load_chat_messages(route);
    }
}
