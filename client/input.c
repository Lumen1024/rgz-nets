#include <input.h>
#include <state.h>
#include <draw.h>
#include <api.h>
#include <commands.h>
#include <ui.h>
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

int utf8_backspace(char *buf, int len)
{
    if (len <= 0)
        return 0;
    int i = len - 1;
    while (i > 0 && (buf[i] & 0xC0) == 0x80)
        i--;
    buf[i] = '\0';
    return i;
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
    if (strncmp(g_current_chat, "/chats/", 7) != 0)
    {
        ui_set_member_list(NULL, 0);
        return;
    }
    const char *p = g_current_chat + 7;
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
    int count = (g_list_mode == LIST_MODE_CHATS) ? g_chat_count : g_user_count;
    if (count == 0)
        return;

    if (g_list_mode == LIST_MODE_CHATS)
    {
        char route[CHAT_ROUTE_LEN];
        snprintf(route, sizeof(route), "/chats/%s/messages",
                 g_chat_names[g_list_selected]);
        g_active = PANEL_CHAT;
        g_focus = PANEL_CHAT;
        load_and_set_chat_messages(route);
    }
    else
    {
        if (g_login[0] && strcmp(g_user_names[g_list_selected], g_login) == 0)
        {
            ui_sys("Cannot open dialog with yourself");
            return;
        }
        char route[MAX_ROUTE_LEN];
        snprintf(route, sizeof(route), "/users/%s/messages",
                 g_user_names[g_list_selected]);
        g_active = PANEL_CHAT;
        g_focus = PANEL_CHAT;
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
        g_active = PANEL_NONE;
        break;
    case KEY_BACKSPACE:
    case 127:
        g_sys_input_len = utf8_backspace(g_sys_input, g_sys_input_len);
        break;
    case 27:
        g_sys_input[0] = '\0';
        g_sys_input_len = 0;
        g_sys_state = SYS_IDLE;
        g_active = PANEL_NONE;
        break;
    default:
        if (ch >= 32 && g_sys_input_len < MAX_TEXT_LEN - 1)
        {
            g_sys_input[g_sys_input_len++] = (char)ch;
            g_sys_input[g_sys_input_len] = '\0';
        }
        break;
    }
}

static void handle_key_nav(int ch)
{
    switch (ch)
    {
    case KEY_LEFT:
        if (g_focus == PANEL_LIST || g_focus == PANEL_SYS)
            g_focus = PANEL_CHAT;
        break;
    case KEY_RIGHT:
        if (g_focus == PANEL_CHAT || g_focus == PANEL_SYS)
            g_focus = PANEL_LIST;
        break;
    case KEY_DOWN:
        if (g_focus != PANEL_SYS)
            g_focus = PANEL_SYS;
        break;
    case KEY_UP:
        if (g_focus == PANEL_SYS)
            g_focus = PANEL_CHAT;
        break;
    case '\n':
    case KEY_ENTER:
        g_active = g_focus;
        if (g_active == PANEL_SYS)
        {
            g_sys_input[0] = '\0';
            g_sys_input_len = 0;
        }
        break;
    default:
        break;
    }
}

static void handle_key_list(int ch)
{
    int count;
    if (g_list_mode == LIST_MODE_CHATS)
        count = g_chat_count;
    else if (g_list_mode == LIST_MODE_USERS)
        count = g_user_count;
    else
        count = g_member_count;

    switch (ch)
    {
    case KEY_UP:
        if (g_list_selected > 0)
            g_list_selected--;
        break;
    case KEY_DOWN:
        if (g_list_selected < count - 1)
            g_list_selected++;
        break;
    case KEY_LEFT:
        if (g_list_mode == LIST_MODE_CHATS)
        {
            g_list_mode = LIST_MODE_MEMBERS;
            g_list_selected = 0;
            load_and_set_member_list();
        }
        else if (g_list_mode == LIST_MODE_USERS)
        {
            g_list_mode = LIST_MODE_CHATS;
            g_list_selected = 0;
            load_and_set_chat_list();
        }
        else
        {
            g_list_mode = LIST_MODE_USERS;
            g_list_selected = 0;
            load_and_set_user_list();
        }
        break;
    case KEY_RIGHT:
        if (g_list_mode == LIST_MODE_CHATS)
        {
            g_list_mode = LIST_MODE_USERS;
            g_list_selected = 0;
            load_and_set_user_list();
        }
        else if (g_list_mode == LIST_MODE_USERS)
        {
            g_list_mode = LIST_MODE_MEMBERS;
            g_list_selected = 0;
            load_and_set_member_list();
        }
        else
        {
            g_list_mode = LIST_MODE_CHATS;
            g_list_selected = 0;
            load_and_set_chat_list();
        }
        break;
    case '\n':
    case KEY_ENTER:
        if (g_list_mode != LIST_MODE_MEMBERS)
            open_selected_item();
        break;
    case 27:
        g_active = PANEL_NONE;
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
        if (g_msg_scroll < g_msg_count)
            g_msg_scroll++;
        break;
    case KEY_DOWN:
        if (g_msg_scroll > 0)
            g_msg_scroll--;
        break;
    case '\n':
    case KEY_ENTER:
        if (g_input_len > 0 && g_current_chat[0])
        {
            if (g_input[0] == '/')
            {
                handle_command(g_input);
            }
            else
            {
                SendArgs *a = malloc(sizeof(SendArgs));
                strncpy(a->route, g_current_chat, sizeof(a->route) - 1);
                strncpy(a->text, g_input, sizeof(a->text) - 1);
                pthread_t tid;
                pthread_create(&tid, NULL, thread_send_message, a);
                pthread_detach(tid);
            }
            g_input[0] = '\0';
            g_input_len = 0;
            g_msg_scroll = 0;
        }
        break;
    case KEY_BACKSPACE:
    case 127:
        g_input_len = utf8_backspace(g_input, g_input_len);
        break;
    case 27:
        g_active = PANEL_NONE;
        break;
    default:
        if (ch >= 32 && g_input_len < MAX_TEXT_LEN - 1)
        {
            g_input[g_input_len++] = (char)ch;
            g_input[g_input_len] = '\0';
        }
        break;
    }
}

void ui_run()
{
    load_and_set_chat_list();
    load_and_set_user_list();
    clearok(stdscr, TRUE);
    draw_all();

    int ch;
    while ((ch = getch()) != 'q')
    {
        if (g_notify_text[0])
        {
            g_notify_text[0] = '\0';
            draw_all();
            continue;
        }

        if (g_active == PANEL_SYS)
            handle_key_sys(ch);
        else if (g_active == PANEL_NONE)
            handle_key_nav(ch);
        else if (g_active == PANEL_LIST)
            handle_key_list(ch);
        else if (g_active == PANEL_CHAT)
            handle_key_chat(ch);

        draw_all();
    }
}
