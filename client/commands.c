#include <commands.h>
#include <state.h>
#include <api.h>
#include <ui.h>
#include <protocol.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static int current_chat_name(char *out, int maxlen)
{
    if (strncmp(g_current_chat, "/chats/", 7) == 0)
    {
        const char *p     = g_current_chat + 7;
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

static int is_private_chat()
{
    return strncmp(g_current_chat, "/users/", 7) == 0;
}

static void reload_chat_list()
{
    char names[MAX_CHATS][MAX_ROUTE_LEN];
    int count = 0;
    if (api_get_chat_list(names, MAX_CHATS, &count) == ERR_OK)
    {
        char *ptrs[MAX_CHATS];
        for (int i = 0; i < count; i++)
            ptrs[i] = names[i];
        ui_set_chat_list(ptrs, count);
    }
}

typedef struct { char chat[MAX_ROUTE_LEN]; char login[MAX_LOGIN_LEN]; } AddUserArgs;
typedef struct { char chat[MAX_ROUTE_LEN]; char login[MAX_LOGIN_LEN]; } RemoveUserArgs;
typedef struct { char chat[MAX_ROUTE_LEN]; } LeaveArgs;
typedef struct { char name[MAX_ROUTE_LEN]; } CreateArgs;

static void *thread_create_chat(void *arg)
{
    CreateArgs *a = arg;
    api_create_chat(a->name);
    free(a);
    return NULL;
}

static void *thread_add_user(void *arg)
{
    AddUserArgs *a = arg;
    api_add_chat_user(a->chat, a->login);
    free(a);
    return NULL;
}

static void *thread_remove_user(void *arg)
{
    RemoveUserArgs *a = arg;
    api_remove_chat_user(a->chat, a->login);
    free(a);
    return NULL;
}

static void *thread_leave_chat(void *arg)
{
    LeaveArgs *a = arg;
    api_leave_chat(a->chat);
    free(a);
    return NULL;
}

void handle_command(const char *cmd)
{
    char arg[MAX_SYS_MSG];
    char chat[MAX_ROUTE_LEN];
    pthread_t tid;

    if (strncmp(cmd, "/create ", 8) == 0)
    {
        const char *name = cmd + 8;
        if (!name[0]) { ui_sys("Usage: /create <chatname>"); return; }
        CreateArgs *a = malloc(sizeof(CreateArgs));
        strncpy(a->name, name, sizeof(a->name) - 1);
        pthread_create(&tid, NULL, thread_create_chat, a);
        pthread_detach(tid);
        snprintf(arg, sizeof(arg), "Creating chat '%s'...", name);
        ui_sys(arg);
        reload_chat_list();
        return;
    }

    if (strncmp(cmd, "/add ", 5) == 0)
    {
        const char *login = cmd + 5;
        if (!login[0]) { ui_sys("Usage: /add <username>"); return; }
        if (!current_chat_name(chat, sizeof(chat))) { ui_sys("Error: not in a group chat"); return; }
        AddUserArgs *a = malloc(sizeof(AddUserArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        strncpy(a->login, login, sizeof(a->login) - 1);
        pthread_create(&tid, NULL, thread_add_user, a);
        pthread_detach(tid);
        snprintf(arg, sizeof(arg), "Adding '%s' to '%s'...", login, chat);
        ui_sys(arg);
        return;
    }

    if (strncmp(cmd, "/delete ", 8) == 0)
    {
        const char *login = cmd + 8;
        if (!login[0]) { ui_sys("Usage: /delete <username>"); return; }
        if (!current_chat_name(chat, sizeof(chat))) { ui_sys("Error: not in a group chat"); return; }
        RemoveUserArgs *a = malloc(sizeof(RemoveUserArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        strncpy(a->login, login, sizeof(a->login) - 1);
        pthread_create(&tid, NULL, thread_remove_user, a);
        pthread_detach(tid);
        snprintf(arg, sizeof(arg), "Removing '%s' from '%s'...", login, chat);
        ui_sys(arg);
        return;
    }

    if (strcmp(cmd, "/leave") == 0)
    {
        if (is_private_chat()) { ui_sys("Error: cannot leave a private dialog"); return; }
        if (!current_chat_name(chat, sizeof(chat))) { ui_sys("Error: not in a group chat"); return; }
        LeaveArgs *a = malloc(sizeof(LeaveArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        pthread_create(&tid, NULL, thread_leave_chat, a);
        pthread_detach(tid);
        g_current_chat[0] = '\0';
        g_msg_count       = 0;
        reload_chat_list();
        ui_sys("Left the chat.");
        return;
    }

    snprintf(arg, sizeof(arg), "Unknown command: %s", cmd);
    ui_sys(arg);
}

void handle_sys_input()
{
    if (g_sys_input_len == 0)
        return;

    if (g_sys_input[0] == '/')
        handle_command(g_sys_input);
    else
        ui_sys(g_sys_input);

    g_sys_input[0]  = '\0';
    g_sys_input_len = 0;
    g_sys_state     = SYS_IDLE;
}
