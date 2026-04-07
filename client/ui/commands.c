#include "commands.h"
#include "state.h"
#include "data.h"

#include <ui.h>
#include <actions.h>
#include <protocol.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

int current_chat_name(char *out, int maxlen)
{
    if (strncmp(g_current_chat, "/chats/", 7) == 0)
    {
        const char *p = g_current_chat + 7;
        const char *slash = strchr(p, '/');
        int len = slash ? (int)(slash - p) : (int)strlen(p);
        if (len >= maxlen)
            return 0;
        strncpy(out, p, len);
        out[len] = '\0';
        return 1;
    }
    return 0;
}

int is_private_chat(void)
{
    return strncmp(g_current_chat, "/users/", 7) == 0;
}

void handle_command(const char *cmd)
{
    char arg[MAX_SYS_MSG];
    char chat[MAX_ROUTE_LEN];

    // /create <chatname>
    if (strncmp(cmd, "/create ", 8) == 0)
    {
        const char *name = cmd + 8;
        if (name[0] == '\0')
        {
            ui_sys("Usage: /create <chatname>");
            return;
        }
        CreateChatArgs *a = malloc(sizeof(CreateChatArgs));
        strncpy(a->name, name, sizeof(a->name) - 1);
        pthread_t tid;
        pthread_create(&tid, NULL, action_create_chat, a);
        pthread_detach(tid);
        snprintf(arg, sizeof(arg), "Creating chat '%s'...", name);
        ui_sys(arg);
        load_chat_list();
        return;
    }

    // /add <username>
    if (strncmp(cmd, "/add ", 5) == 0)
    {
        const char *login = cmd + 5;
        if (login[0] == '\0')
        {
            ui_sys("Usage: /add <username>");
            return;
        }
        if (!current_chat_name(chat, sizeof(chat)))
        {
            ui_sys("Error: not in a group chat");
            return;
        }
        ChatUserArgs *a = malloc(sizeof(ChatUserArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        strncpy(a->login, login, sizeof(a->login) - 1);
        pthread_t tid;
        pthread_create(&tid, NULL, action_add_chat_user, a);
        pthread_detach(tid);
        snprintf(arg, sizeof(arg), "Adding '%s' to '%s'...", login, chat);
        ui_sys(arg);
        return;
    }

    // /delete <username>
    if (strncmp(cmd, "/delete ", 8) == 0)
    {
        const char *login = cmd + 8;
        if (login[0] == '\0')
        {
            ui_sys("Usage: /delete <username>");
            return;
        }
        if (!current_chat_name(chat, sizeof(chat)))
        {
            ui_sys("Error: not in a group chat");
            return;
        }
        ChatUserArgs *a = malloc(sizeof(ChatUserArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        strncpy(a->login, login, sizeof(a->login) - 1);
        pthread_t tid;
        pthread_create(&tid, NULL, action_remove_chat_user, a);
        pthread_detach(tid);
        snprintf(arg, sizeof(arg), "Removing '%s' from '%s'...", login, chat);
        ui_sys(arg);
        return;
    }

    // /leave
    if (strcmp(cmd, "/leave") == 0)
    {
        if (is_private_chat())
        {
            ui_sys("Error: cannot leave a private dialog");
            return;
        }
        if (!current_chat_name(chat, sizeof(chat)))
        {
            ui_sys("Error: not in a group chat");
            return;
        }
        LeaveChatArgs *a = malloc(sizeof(LeaveChatArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        pthread_t tid;
        pthread_create(&tid, NULL, action_leave_chat, a);
        pthread_detach(tid);
        g_current_chat[0] = '\0';
        g_msg_count = 0;
        load_chat_list();
        ui_sys("Left the chat.");
        return;
    }

    snprintf(arg, sizeof(arg), "Unknown command: %s", cmd);
    ui_sys(arg);
}

void handle_sys_input(void)
{
    if (g_sys_input_len == 0)
        return;

    if (g_sys_input[0] == '/')
    {
        handle_command(g_sys_input);
    }
    else
    {
        ui_sys(g_sys_input);
    }

    g_sys_input[0] = '\0';
    g_sys_input_len = 0;
    g_sys_state = SYS_IDLE;
}
