#include <message_handler.h>

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cJSON.h>
#include <protocol.h>
#include <response.h>
#include <notification.h>
#include <notify.h>
#include <message_repository.h>
#include <chat_repository.h>
#include <user_repository.h>

Response handle_get_chat_messages(const char *chat) {
    if (!repo_chat_exists(chat)) {
        return make_error(ERR_NOT_FOUND);
    }

    Message *msgs = NULL;
    int count = 0;

    if (repo_msg_get_chat(chat, &msgs, &count) != 0) {
        return make_error(ERR_INTERNAL);
    }

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "login", msgs[i].login);
        cJSON_AddStringToObject(obj, "text", msgs[i].text);
        cJSON_AddStringToObject(obj, "timestamp", msgs[i].timestamp);
        cJSON_AddItemToArray(arr, obj);
    }
    free(msgs);

    return make_success(arr);
}

Response handle_post_chat_message(const char *chat, Request *req, const char *login) {
    if (!req->content) {
        return make_error(ERR_BAD_REQUEST);
    }

    cJSON *text_j = cJSON_GetObjectItem(req->content, "text");
    if (!cJSON_IsString(text_j) || strlen(text_j->valuestring) == 0) {
        return make_error(ERR_BAD_REQUEST);
    }

    if (!repo_chat_exists(chat)) {
        return make_error(ERR_NOT_FOUND);
    }

    if (repo_msg_save_chat(chat, login, text_j->valuestring) != 0) {
        return make_error(ERR_INTERNAL);
    }

    // Notify all chat members about the new message
    cJSON *notif_body = cJSON_CreateObject();
    cJSON_AddStringToObject(notif_body, "login", login);
    cJSON_AddStringToObject(notif_body, "text",  text_j->valuestring);
    cJSON_AddStringToObject(notif_body, "chat",  chat);

    char ts[MAX_TIMESTAMP_LEN];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    cJSON_AddStringToObject(notif_body, "timestamp", ts);

    Notification notif;
    notif.kind    = MSG_NOTIFICATION;
    notif.code    = NOTIF_NEW_MESSAGE;
    notif.content = notif_body;
    notify_chat(chat, notif);
    cJSON_Delete(notif_body);

    return make_success(NULL);
}

Response handle_get_private_messages(const char *login_a, const char *login_b) {
    if (!repo_user_exists(login_b)) {
        return make_error(ERR_NOT_FOUND);
    }

    Message *msgs = NULL;
    int count = 0;

    if (repo_msg_get_private(login_a, login_b, &msgs, &count) != 0) {
        return make_error(ERR_INTERNAL);
    }

    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "login", msgs[i].login);
        cJSON_AddStringToObject(obj, "text", msgs[i].text);
        cJSON_AddStringToObject(obj, "timestamp", msgs[i].timestamp);
        cJSON_AddItemToArray(arr, obj);
    }
    free(msgs);

    return make_success(arr);
}

Response handle_post_private_message(const char *to, Request *req, const char *login) {
    if (!req->content) {
        return make_error(ERR_BAD_REQUEST);
    }

    cJSON *text_j = cJSON_GetObjectItem(req->content, "text");
    if (!cJSON_IsString(text_j) || strlen(text_j->valuestring) == 0) {
        return make_error(ERR_BAD_REQUEST);
    }

    if (!repo_user_exists(to)) {
        return make_error(ERR_NOT_FOUND);
    }

    if (repo_msg_save_private(login, to, text_j->valuestring) != 0) {
        return make_error(ERR_INTERNAL);
    }

    // Notify sender and recipient about the new private message
    cJSON *notif_body = cJSON_CreateObject();
    cJSON_AddStringToObject(notif_body, "login", login);
    cJSON_AddStringToObject(notif_body, "text",  text_j->valuestring);
    cJSON_AddStringToObject(notif_body, "to",    to);

    char ts[MAX_TIMESTAMP_LEN];
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
    cJSON_AddStringToObject(notif_body, "timestamp", ts);

    Notification notif;
    notif.kind    = MSG_NOTIFICATION;
    notif.code    = NOTIF_NEW_MESSAGE;
    notif.content = notif_body;
    notify_user(to,    notif);
    notify_user(login, notif);
    cJSON_Delete(notif_body);

    return make_success(NULL);
}
