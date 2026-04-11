#include <input.h>
#include <ui.h>
#include <api.h>
#include <commands.h>
#include <protocol.h>

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

typedef struct
{
    char route[MAX_ROUTE_LEN];
    char text[MAX_TEXT_LEN];
} SendArgs;

static void *thread_send_message(void *arg)
{
    SendArgs *a = arg;
    api_send_message(a->route, a->text);
    free(a);
    return NULL;
}

static void load_and_set_chat_messages(const char *route)
{
    Message msgs[MAX_MESSAGES];
    int count = 0;
    if (api_get_chat_messages(route, msgs, MAX_MESSAGES, &count) == ERR_OK)
        ui_set_chat(route, msgs, count);
}

static void load_and_set_chat_list()
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

static void load_and_set_user_list()
{
    char names[MAX_USERS][MAX_LOGIN_LEN];
    int count = 0;
    if (api_get_user_list(names, MAX_USERS, &count) == ERR_OK)
    {
        char *ptrs[MAX_USERS];
        int has_msg[MAX_USERS];
        for (int i = 0; i < count; i++)
        {
            ptrs[i] = names[i];
            has_msg[i] = 0;
        }
        ui_set_user_list(ptrs, has_msg, count);
    }
}

static void load_and_set_member_list()
{
    const char *chat = ui_get_current_chat();
    if (!chat || strncmp(chat, "/chats/", 7) != 0)
    {
        ui_set_member_list(NULL, 0);
        return;
    }
    const char *p = chat + 7;
    const char *slash = strchr(p, '/');
    int len = slash ? (int)(slash - p) : (int)strlen(p);
    char chat_name[MAX_ROUTE_LEN];
    strncpy(chat_name, p, len);
    chat_name[len] = '\0';

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

static void open_selected_item()
{
    int idx = ui_get_list_selected();
    int mode = ui_get_list_mode();

    if (mode == LIST_MODE_CHATS)
    {
        const char *name = ui_get_list_item(idx);
        if (!name)
            return;
        char route[CHAT_ROUTE_LEN];
        snprintf(route, sizeof(route), "/chats/%s/messages", name);
        ui_set_focus(PANEL_CHAT);
        ui_set_active(PANEL_CHAT);
        load_and_set_chat_messages(route);
    }
    else if (mode == LIST_MODE_USERS)
    {
        const char *name = ui_get_list_item(idx);
        if (!name)
            return;
        const char *me = api_get_login();
        if (me && strcmp(name, me) == 0)
        {
            ui_sys("Cannot open dialog with yourself");
            return;
        }
        char route[MAX_ROUTE_LEN];
        snprintf(route, sizeof(route), "/users/%s/messages", name);
        ui_set_focus(PANEL_CHAT);
        ui_set_active(PANEL_CHAT);
        load_and_set_chat_messages(route);
    }
}

static void handle_key_sys(int ch)
{
    switch (ch)
    {
    case '\n':
    case KEY_ENTER:
        handle_sys_input();
        ui_set_active(PANEL_NONE);
        break;
    case KEY_BACKSPACE:
    case 127:
        ui_sys_input_backspace();
        break;
    case 27:
        ui_sys_input_clear();
        ui_set_active(PANEL_NONE);
        break;
    default:
        if (ch >= 32)
            ui_sys_input_append((char)ch);
        break;
    }
}

static void handle_key_nav(int ch)
{
    Panel focus = ui_get_focus();
    switch (ch)
    {
    case KEY_LEFT:
        if (focus == PANEL_LIST || focus == PANEL_SYS)
            ui_set_focus(PANEL_CHAT);
        break;
    case KEY_RIGHT:
        if (focus == PANEL_CHAT || focus == PANEL_SYS)
            ui_set_focus(PANEL_LIST);
        break;
    case KEY_DOWN:
        if (focus != PANEL_SYS)
            ui_set_focus(PANEL_SYS);
        break;
    case KEY_UP:
        if (focus == PANEL_SYS)
            ui_set_focus(PANEL_CHAT);
        break;
    case '\n':
    case KEY_ENTER:
        ui_set_active(focus);
        if (ui_get_active() == PANEL_SYS)
            ui_sys_input_clear();
        break;
    default:
        break;
    }
}

static void handle_key_list(int ch)
{
    int count = ui_get_list_count();
    int sel = ui_get_list_selected();
    ListMode mode = ui_get_list_mode();

    switch (ch)
    {
    case KEY_UP:
        if (sel > 0)
            ui_set_list_selected(sel - 1);
        break;
    case KEY_DOWN:
        if (sel < count - 1)
            ui_set_list_selected(sel + 1);
        break;
    case KEY_LEFT:
        if (mode == LIST_MODE_CHATS)
        {
            ui_set_list_mode(LIST_MODE_MEMBERS);
            load_and_set_member_list();
        }
        else if (mode == LIST_MODE_USERS)
        {
            ui_set_list_mode(LIST_MODE_CHATS);
            load_and_set_chat_list();
        }
        else
        {
            ui_set_list_mode(LIST_MODE_USERS);
            load_and_set_user_list();
        }
        break;
    case KEY_RIGHT:
        if (mode == LIST_MODE_CHATS)
        {
            ui_set_list_mode(LIST_MODE_USERS);
            load_and_set_user_list();
        }
        else if (mode == LIST_MODE_USERS)
        {
            ui_set_list_mode(LIST_MODE_MEMBERS);
            load_and_set_member_list();
        }
        else
        {
            ui_set_list_mode(LIST_MODE_CHATS);
            load_and_set_chat_list();
        }
        break;
    case '\n':
    case KEY_ENTER:
        if (mode != LIST_MODE_MEMBERS)
            open_selected_item();
        break;
    case 27:
        ui_set_active(PANEL_NONE);
        break;
    default:
        break;
    }
}

static void handle_key_chat(int ch)
{
    switch (ch)
    {
    case KEY_UP:
        ui_scroll_up();
        break;
    case KEY_DOWN:
        ui_scroll_down();
        break;
    case '\n':
    case KEY_ENTER:
        if (ui_get_input_len() > 0 && ui_get_current_chat())
        {
            const char *input = ui_get_input();
            if (input[0] == '/')
            {
                handle_command(input);
            }
            else
            {
                SendArgs *a = malloc(sizeof(SendArgs));
                strncpy(a->route, ui_get_current_chat(), sizeof(a->route) - 1);
                strncpy(a->text, input, sizeof(a->text) - 1);
                pthread_t tid;
                pthread_create(&tid, NULL, thread_send_message, a);
                pthread_detach(tid);
            }
            ui_input_clear();
        }
        break;
    case KEY_BACKSPACE:
    case 127:
        ui_input_backspace();
        break;
    case 27:
        ui_set_active(PANEL_NONE);
        break;
    default:
        if (ch >= 32)
            ui_input_append((char)ch);
        break;
    }
}

void ui_run()
{
    load_and_set_chat_list();
    load_and_set_user_list();
    clearok(stdscr, TRUE);
    ui_redraw();

    int ch;
    while ((ch = getch()) != 'q')
    {
        if (ui_has_notify())
        {
            ui_clear_notify();
            ui_redraw();
            continue;
        }

        Panel active = ui_get_active();
        if (active == PANEL_SYS)
            handle_key_sys(ch);
        else if (active == PANEL_NONE)
            handle_key_nav(ch);
        else if (active == PANEL_LIST)
            handle_key_list(ch);
        else if (active == PANEL_CHAT)
            handle_key_chat(ch);

        ui_redraw();
    }
}
