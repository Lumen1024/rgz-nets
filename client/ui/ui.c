#include <ui.h>
#include <state.h>
#include <draw.h>

#include <ncurses.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>

#define SYS_MSG_TTL 4

static int utf8_backspace(char *buf, int len)
{
    if (len <= 0)
        return 0;
    int i = len - 1;
    while (i > 0 && (buf[i] & 0xC0) == 0x80)
        i--;
    buf[i] = '\0';
    return i;
}

void ui_init()
{
    set_escdelay(25);
    initscr();
    halfdelay(10);
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    start_color();
    init_pair(CP_DEFAULT, COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_SELECTED, COLOR_YELLOW, COLOR_BLACK);
    init_pair(CP_ACTIVE, COLOR_GREEN, COLOR_BLACK);
    init_pair(CP_NOTIFY, COLOR_BLACK, COLOR_YELLOW);
    init_pair(CP_SYS, COLOR_CYAN, COLOR_BLACK);
    init_pair(CP_DIM, COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_SYS_OK, COLOR_GREEN, COLOR_BLACK);
    init_pair(CP_SYS_ERR, COLOR_RED, COLOR_BLACK);

    getmaxyx(stdscr, g_rows, g_cols);
    g_main_h = g_rows - SYS_BAR_H;
    g_left_w = (g_cols * 7) / 10;
    g_right_w = g_cols - g_left_w;

    g_win_chat = newwin(g_main_h, g_left_w, 0, 0);
    g_win_list = newwin(g_main_h, g_right_w, 0, g_left_w);
    g_win_chat_in = newwin(g_main_h - 2, g_left_w - 2, 1, 1);
    g_win_list_in = newwin(g_main_h - 2, g_right_w - 2, 1, g_left_w + 1);
    g_win_sys = newwin(SYS_BAR_H, g_cols, g_main_h, 0);

    scrollok(g_win_chat_in, FALSE);
    scrollok(g_win_list_in, FALSE);

    refresh();
}

void ui_destroy()
{
    if (g_win_chat_in)
    {
        delwin(g_win_chat_in);
        g_win_chat_in = NULL;
    }
    if (g_win_list_in)
    {
        delwin(g_win_list_in);
        g_win_list_in = NULL;
    }
    if (g_win_chat)
    {
        delwin(g_win_chat);
        g_win_chat = NULL;
    }
    if (g_win_list)
    {
        delwin(g_win_list);
        g_win_list = NULL;
    }
    if (g_win_sys)
    {
        delwin(g_win_sys);
        g_win_sys = NULL;
    }
    endwin();
}

void ui_redraw()
{
    draw_all();
}

void read_line(const char *prompt, char *out, int maxlen, int hidden)
{
    printf("%s", prompt);
    fflush(stdout);

    if (hidden)
    {
        struct termios old, noecho;
        tcgetattr(STDIN_FILENO, &old);
        noecho = old;
        noecho.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &noecho);
        if (fgets(out, maxlen, stdin))
            out[strcspn(out, "\n")] = '\0';
        tcsetattr(STDIN_FILENO, TCSANOW, &old);
        printf("\n");
    }
    else
    {
        if (fgets(out, maxlen, stdin))
            out[strcspn(out, "\n")] = '\0';
    }
}

int ui_prompt_server(char *host_out, int *port_out)
{
    char buf[128];
    read_line("Server address (host:port): ", buf, sizeof(buf), 0);
    char *colon = strrchr(buf, ':');
    if (colon)
    {
        *colon = '\0';
        strncpy(host_out, buf, 63);
        *port_out = atoi(colon + 1);
    }
    else
    {
        strncpy(host_out, buf, 63);
        *port_out = 8080;
    }
    return 0;
}

int ui_prompt_login(char *login_out, char *password_out)
{
    read_line("Login: ", login_out, 64, 0);
    read_line("Password: ", password_out, 64, 1);
    return 0;
}

int ui_prompt_register(char *login_out, char *password_out)
{
    read_line("New login: ", login_out, 64, 0);
    read_line("New password: ", password_out, 64, 1);
    return 0;
}

void ui_set_chat(const char *chat_name, Message *msgs, int count)
{
    pthread_mutex_lock(&g_ui_mutex);
    strncpy(g_current_chat, chat_name, MAX_ROUTE_LEN - 1);
    g_msg_count = count < MAX_MESSAGES ? count : MAX_MESSAGES;
    g_msg_scroll = 0;
    for (int i = 0; i < g_msg_count; i++)
        g_messages[i] = msgs[i];
    pthread_mutex_unlock(&g_ui_mutex);
    draw_all();
}

void ui_set_chat_list(char **names, int count)
{
    pthread_mutex_lock(&g_ui_mutex);
    g_chat_count = count < MAX_CHATS ? count : MAX_CHATS;
    for (int i = 0; i < g_chat_count; i++)
        strncpy(g_chat_names[i], names[i], MAX_ROUTE_LEN - 1);
    g_list_selected = 0;
    pthread_mutex_unlock(&g_ui_mutex);
    draw_all();
}

void ui_set_member_list(char **names, int count)
{
    pthread_mutex_lock(&g_ui_mutex);
    g_member_count = count < MAX_MEMBERS ? count : MAX_MEMBERS;
    if (names)
        for (int i = 0; i < g_member_count; i++)
            strncpy(g_member_names[i], names[i], MAX_LOGIN_LEN - 1);
    pthread_mutex_unlock(&g_ui_mutex);
    draw_all();
}

void ui_set_user_list(char **names, int *has_messages, int count)
{
    pthread_mutex_lock(&g_ui_mutex);
    g_user_count = count < MAX_USERS ? count : MAX_USERS;
    for (int i = 0; i < g_user_count; i++)
    {
        strncpy(g_user_names[i], names[i], MAX_LOGIN_LEN - 1);
        g_user_has_msg[i] = has_messages[i];
    }
    g_list_selected = 0;
    pthread_mutex_unlock(&g_ui_mutex);
    draw_all();
}

void ui_append_message(Message *msg)
{
    pthread_mutex_lock(&g_ui_mutex);
    if (g_msg_count < MAX_MESSAGES)
        g_messages[g_msg_count++] = *msg;
    pthread_mutex_unlock(&g_ui_mutex);
    draw_chat_panel();
    wrefresh(g_win_chat);
    wrefresh(g_win_chat_in);
}

void ui_clear_chat()
{
    pthread_mutex_lock(&g_ui_mutex);
    g_current_chat[0] = '\0';
    g_msg_count = 0;
    g_msg_scroll = 0;
    pthread_mutex_unlock(&g_ui_mutex);
}

void ui_notify(const char *text)
{
    ui_sys_typed(text, SYS_INFO);
}

void ui_sys(const char *text)
{
    ui_sys_typed(text, SYS_INFO);
}

void ui_sys_typed(const char *text, SysMsgType type)
{
    pthread_mutex_lock(&g_ui_mutex);
    strncpy(g_sys_msg, text, MAX_SYS_MSG - 1);
    g_sys_msg_time = time(NULL);
    g_sys_msg_type = type;
    pthread_mutex_unlock(&g_ui_mutex);
    draw_sys_bar();
    doupdate();
}

void ui_sys_tick()
{
    if (g_sys_msg[0] && time(NULL) - g_sys_msg_time >= SYS_MSG_TTL)
    {
        pthread_mutex_lock(&g_ui_mutex);
        g_sys_msg[0] = '\0';
        pthread_mutex_unlock(&g_ui_mutex);
        draw_sys_bar();
        doupdate();
    }
}

const char *ui_get_current_chat()
{
    return g_current_chat[0] ? g_current_chat : NULL;
}

Panel ui_get_focus() { return g_focus; }
Panel ui_get_active() { return g_active; }

void ui_set_focus(Panel p) { g_focus = p; }
void ui_set_active(Panel p) { g_active = p; }

ListMode ui_get_list_mode() { return g_list_mode; }

void ui_set_list_mode(ListMode mode)
{
    g_list_mode = mode;
    g_list_selected = 0;
}

int ui_get_list_selected() { return g_list_selected; }
void ui_set_list_selected(int i) { g_list_selected = i; }

int ui_get_list_count()
{
    if (g_list_mode == LIST_MODE_CHATS)
        return g_chat_count;
    if (g_list_mode == LIST_MODE_USERS)
        return g_user_count;
    return g_member_count;
}

const char *ui_get_list_item(int idx)
{
    if (g_list_mode == LIST_MODE_CHATS && idx < g_chat_count)
        return g_chat_names[idx];
    if (g_list_mode == LIST_MODE_USERS && idx < g_user_count)
        return g_user_names[idx];
    if (g_list_mode == LIST_MODE_MEMBERS && idx < g_member_count)
        return g_member_names[idx];
    return NULL;
}

void ui_scroll_up()
{
    if (g_msg_scroll < g_msg_count)
        g_msg_scroll++;
}

void ui_scroll_down()
{
    if (g_msg_scroll > 0)
        g_msg_scroll--;
}

const char *ui_get_input() { return g_input; }
int ui_get_input_len() { return g_input_len; }

void ui_input_append(char ch)
{
    if (g_input_len < MAX_TEXT_LEN - 1)
    {
        g_input[g_input_len++] = ch;
        g_input[g_input_len] = '\0';
    }
}

void ui_input_backspace()
{
    g_input_len = utf8_backspace(g_input, g_input_len);
}

void ui_input_clear()
{
    g_input[0] = '\0';
    g_input_len = 0;
}

