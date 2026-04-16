#include <commands.h>
#include <ui.h>
#include <api.h>
#include <protocol.h>
#include <config.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

static int current_chat_name(char *out, int maxlen)
{
    const char *chat = ui_get_current_chat();
    if (!chat || strncmp(chat, "/chats/", 7) != 0)
        return 0;
    const char *p = chat + 7;
    const char *slash = strchr(p, '/');
    int len = slash ? (int)(slash - p) : (int)strlen(p);
    if (len >= maxlen)
        return 0;
    strncpy(out, p, len);
    out[len] = '\0';
    return 1;
}

static int is_private_chat()
{
    const char *chat = ui_get_current_chat();
    return chat && strncmp(chat, "/users/", 7) == 0;
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


typedef struct { char name[MAX_ROUTE_LEN]; } CreateArgs;
typedef struct { char name[MAX_ROUTE_LEN]; } DeleteChatArgs;
typedef struct { char chat[MAX_ROUTE_LEN]; char login[MAX_LOGIN_LEN]; } AddUserArgs;
typedef struct { char chat[MAX_ROUTE_LEN]; char login[MAX_LOGIN_LEN]; } RemoveUserArgs;
typedef struct { char chat[MAX_ROUTE_LEN]; } LeaveArgs;

static void *thread_create_chat(void *arg)
{
    CreateArgs *a = arg;
    int code = api_create_chat(a->name);
    if (code == ERR_OK)
    {
        char msg[MAX_SYS_MSG];
        snprintf(msg, sizeof(msg), "Chat '%s' created", a->name);
        ui_sys_typed(msg, SYS_OK);
        reload_chat_list();
    }
    else
    {
        ui_sys_typed("Failed to create chat", SYS_ERR);
    }
    free(a);
    return NULL;
}

static void *thread_delete_chat(void *arg)
{
    DeleteChatArgs *a = arg;
    int code = api_delete_chat(a->name);
    if (code == ERR_OK)
    {
        char msg[MAX_SYS_MSG];
        snprintf(msg, sizeof(msg), "Chat '%s' deleted", a->name);
        ui_sys_typed(msg, SYS_OK);
        reload_chat_list();
    }
    else if (code == ERR_FORBIDDEN)
    {
        ui_sys_typed("Error: only the owner can delete the chat", SYS_ERR);
    }
    else
    {
        ui_sys_typed("Failed to delete chat", SYS_ERR);
    }
    free(a);
    return NULL;
}

static void *thread_add_user(void *arg)
{
    AddUserArgs *a = arg;
    int code = api_add_chat_user(a->chat, a->login);
    if (code == ERR_OK)
    {
        char msg[MAX_SYS_MSG];
        snprintf(msg, sizeof(msg), "Added '%s' to '%s'", a->login, a->chat);
        ui_sys_typed(msg, SYS_OK);
    }
    else
    {
        ui_sys_typed("Failed to add user", SYS_ERR);
    }
    free(a);
    return NULL;
}

static void *thread_remove_user(void *arg)
{
    RemoveUserArgs *a = arg;
    int code = api_remove_chat_user(a->chat, a->login);
    if (code == ERR_OK)
    {
        char msg[MAX_SYS_MSG];
        snprintf(msg, sizeof(msg), "Removed '%s' from '%s'", a->login, a->chat);
        ui_sys_typed(msg, SYS_OK);
    }
    else
    {
        ui_sys_typed("Failed to remove user", SYS_ERR);
    }
    free(a);
    return NULL;
}

static void *thread_leave_chat(void *arg)
{
    LeaveArgs *a = arg;
    int code = api_leave_chat(a->chat);
    if (code == ERR_OK)
        ui_sys_typed("Left the chat", SYS_OK);
    else
        ui_sys_typed("Failed to leave chat", SYS_ERR);
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
        if (!name[0])
        {
            ui_sys_typed("Usage: /create <chatname>", SYS_ERR);
            return;
        }
        CreateArgs *a = malloc(sizeof(CreateArgs));
        strncpy(a->name, name, sizeof(a->name) - 1);
        pthread_create(&tid, NULL, thread_create_chat, a);
        pthread_detach(tid);
        return;
    }

    if (strncmp(cmd, "/add ", 5) == 0)
    {
        const char *login = cmd + 5;
        if (!login[0])
        {
            ui_sys_typed("Usage: /add <username>", SYS_ERR);
            return;
        }
        if (!current_chat_name(chat, sizeof(chat)))
        {
            ui_sys_typed("Error: not in a group chat", SYS_ERR);
            return;
        }
        AddUserArgs *a = malloc(sizeof(AddUserArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        strncpy(a->login, login, sizeof(a->login) - 1);
        pthread_create(&tid, NULL, thread_add_user, a);
        pthread_detach(tid);
        return;
    }

    if (strncmp(cmd, "/delete ", 8) == 0)
    {
        const char *name = cmd + 8;
        if (!name[0])
        {
            ui_sys_typed("Usage: /delete <chatname>", SYS_ERR);
            return;
        }
        DeleteChatArgs *a = malloc(sizeof(DeleteChatArgs));
        strncpy(a->name, name, sizeof(a->name) - 1);
        pthread_create(&tid, NULL, thread_delete_chat, a);
        pthread_detach(tid);
        char cur_name[MAX_ROUTE_LEN];
        if (current_chat_name(cur_name, sizeof(cur_name)) && strcmp(cur_name, name) == 0)
            ui_clear_chat();
        return;
    }

    if (strncmp(cmd, "/delete-member ", 15) == 0)
    {
        const char *login = cmd + 15;
        if (!login[0])
        {
            ui_sys_typed("Usage: /delete-member <username>", SYS_ERR);
            return;
        }
        if (!current_chat_name(chat, sizeof(chat)))
        {
            ui_sys_typed("Error: not in a group chat", SYS_ERR);
            return;
        }
        RemoveUserArgs *a = malloc(sizeof(RemoveUserArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        strncpy(a->login, login, sizeof(a->login) - 1);
        pthread_create(&tid, NULL, thread_remove_user, a);
        pthread_detach(tid);
        return;
    }

    if (strcmp(cmd, "/leave") == 0)
    {
        if (is_private_chat())
        {
            ui_sys_typed("Error: cannot leave a private dialog", SYS_ERR);
            return;
        }
        if (!current_chat_name(chat, sizeof(chat)))
        {
            ui_sys_typed("Error: not in a group chat", SYS_ERR);
            return;
        }
        LeaveArgs *a = malloc(sizeof(LeaveArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        pthread_create(&tid, NULL, thread_leave_chat, a);
        pthread_detach(tid);
        ui_clear_chat();
        reload_chat_list();
        return;
    }

    snprintf(arg, sizeof(arg), "Unknown command: %s", cmd);
    ui_sys_typed(arg, SYS_ERR);
}

