#pragma once

typedef struct
{
    char login[64];
    char password[64];
} AuthArgs;

typedef struct
{
    char chat[256];
    char text[1024];
} SendMessageArgs;

typedef struct
{
    char name[256];
} CreateChatArgs;

typedef struct
{
    char chat[128];
    char login[64];
} ChatUserArgs;

typedef struct
{
    char chat[128];
} LeaveChatArgs;

typedef struct
{
    char to[64];
    char filepath[512];
} SendFileArgs;

const char *actions_get_token(void);
const char *actions_get_login(void);

void *action_login(void *args);
void *action_register(void *args);
void *action_send_message(void *args);
void *action_create_chat(void *args);
void *action_add_chat_user(void *args);
void *action_remove_chat_user(void *args);
void *action_leave_chat(void *args);
void *action_send_file(void *args);
