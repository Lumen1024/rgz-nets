#include <input.h>
#include <state.h>
#include <draw.h>
#include <api.h>
#include <commands.h>
#include <ui.h>
#include <actions.h>
#include <protocol.h>

#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>

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

void ui_run(void)
{
    load_chat_list();
    load_user_list();
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
                g_sys_input[0]  = '\0';
                g_sys_input_len = 0;
                g_sys_state     = SYS_IDLE;
                g_active        = PANEL_NONE;
                break;
            default:
                if (ch >= 32 && g_sys_input_len < MAX_TEXT_LEN - 1)
                {
                    g_sys_input[g_sys_input_len++] = (char)ch;
                    g_sys_input[g_sys_input_len]   = '\0';
                }
                break;
            }
            draw_all();
            continue;
        }

        if (g_active == PANEL_NONE)
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
                    g_sys_input[0]  = '\0';
                    g_sys_input_len = 0;
                }
                break;
            default:
                break;
            }
            draw_all();
            continue;
        }

        if (g_active == PANEL_LIST)
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
                    g_list_mode     = LIST_MODE_MEMBERS;
                    g_list_selected = 0;
                    load_member_list();
                }
                else if (g_list_mode == LIST_MODE_USERS)
                {
                    g_list_mode     = LIST_MODE_CHATS;
                    g_list_selected = 0;
                    load_chat_list();
                }
                else
                {
                    g_list_mode     = LIST_MODE_USERS;
                    g_list_selected = 0;
                    load_user_list();
                }
                break;
            case KEY_RIGHT:
                if (g_list_mode == LIST_MODE_CHATS)
                {
                    g_list_mode     = LIST_MODE_USERS;
                    g_list_selected = 0;
                    load_user_list();
                }
                else if (g_list_mode == LIST_MODE_USERS)
                {
                    g_list_mode     = LIST_MODE_MEMBERS;
                    g_list_selected = 0;
                    load_member_list();
                }
                else
                {
                    g_list_mode     = LIST_MODE_CHATS;
                    g_list_selected = 0;
                    load_chat_list();
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
            draw_all();
            continue;
        }

        if (g_active == PANEL_CHAT)
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
                        SendMessageArgs *a = malloc(sizeof(SendMessageArgs));
                        strncpy(a->chat, g_current_chat, sizeof(a->chat) - 1);
                        strncpy(a->text, g_input, sizeof(a->text) - 1);
                        pthread_t tid;
                        pthread_create(&tid, NULL, action_send_message, a);
                        pthread_detach(tid);
                    }
                    g_input[0]   = '\0';
                    g_input_len  = 0;
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
                    g_input[g_input_len]   = '\0';
                }
                break;
            }
            draw_all();
            continue;
        }
    }
}
