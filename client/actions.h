#pragma once

typedef struct {
    char login[64];
    char password[64];
} AuthArgs;

typedef struct {
    char chat[256];
    char text[1024];
} SendMessageArgs;

typedef struct {
    char name[256];
} CreateChatArgs;

typedef struct {
    char to[64];
    char filepath[512];
} SendFileArgs;

void *action_login(void *args);
void *action_register(void *args);
void *action_send_message(void *args);
void *action_create_chat(void *args);
void *action_send_file(void *args);
