#include <draw.h>

#include <string.h>
#include <stdio.h>

int get_border_color(Panel panel)
{
    if (g_active == panel)
        return CP_ACTIVE;
    if (g_focus == panel)
        return CP_SELECTED;
    return CP_DEFAULT;
}

void draw_chat_panel(void)
{
    int cp = get_border_color(PANEL_CHAT);
    wattron(g_win_chat, COLOR_PAIR(cp));
    box(g_win_chat, 0, 0);

    char title_buf[MAX_ROUTE_LEN];
    const char *title;
    if (!g_current_chat[0])
    {
        title = "[ no chat ]";
    }
    else if (strncmp(g_current_chat, "/chats/", 7) == 0)
    {
        const char *p = g_current_chat + 7;
        const char *slash = strchr(p, '/');
        int len = slash ? (int)(slash - p) : (int)strlen(p);
        strncpy(title_buf, p, len);
        title_buf[len] = '\0';
        title = title_buf;
    }
    else if (strncmp(g_current_chat, "/users/", 7) == 0)
    {
        const char *p = g_current_chat + 7;
        const char *slash = strchr(p, '/');
        int len = slash ? (int)(slash - p) : (int)strlen(p);
        strncpy(title_buf, p, len);
        title_buf[len] = '\0';
        title = title_buf;
    }
    else
    {
        title = g_current_chat;
    }
    mvwprintw(g_win_chat, 0, 2, " %s ", title);
    wattroff(g_win_chat, COLOR_PAIR(cp));

    int inner_h, inner_w;
    getmaxyx(g_win_chat_in, inner_h, inner_w);
    (void)inner_w;

    int msg_h = inner_h - 2;

    werase(g_win_chat_in);

    int visible = msg_h;
    int total   = g_msg_count;
    int start   = total - visible - g_msg_scroll;
    if (start < 0)
        start = 0;
    int end = start + visible;
    if (end > total)
        end = total;

    for (int i = start; i < end; i++)
    {
        int row = i - start;
        mvwprintw(g_win_chat_in, row, 0, "[%s] %s",
                  g_messages[i].login, g_messages[i].text);
    }

    mvwhline(g_win_chat_in, msg_h, 0, ACS_HLINE, inner_w);

    if (g_active == PANEL_CHAT)
    {
        curs_set(1);
        mvwprintw(g_win_chat_in, msg_h + 1, 0, "> %s", g_input);
        wmove(g_win_chat_in, msg_h + 1, 2 + g_input_len);
    }
    else
    {
        curs_set(0);
        mvwprintw(g_win_chat_in, msg_h + 1, 0, "> ");
    }

    wnoutrefresh(g_win_chat);
    wnoutrefresh(g_win_chat_in);
}

void draw_list_panel(void)
{
    int cp = get_border_color(PANEL_LIST);
    wattron(g_win_list, COLOR_PAIR(cp));
    box(g_win_list, 0, 0);

    if (g_list_mode == LIST_MODE_CHATS)
        mvwprintw(g_win_list, 0, 2, " Chats [</>] ");
    else if (g_list_mode == LIST_MODE_USERS)
        mvwprintw(g_win_list, 0, 2, " Users [</>] ");
    else
        mvwprintw(g_win_list, 0, 2, " Members [</>] ");
    wattroff(g_win_list, COLOR_PAIR(cp));

    int inner_h, inner_w;
    getmaxyx(g_win_list_in, inner_h, inner_w);
    (void)inner_w;

    werase(g_win_list_in);

    int count;
    if (g_list_mode == LIST_MODE_CHATS)
        count = g_chat_count;
    else if (g_list_mode == LIST_MODE_USERS)
        count = g_user_count;
    else
        count = g_member_count;

    int offset = 0;
    if (g_list_selected >= inner_h)
        offset = g_list_selected - inner_h + 1;

    for (int i = 0; i < count && (i - offset) < inner_h; i++)
    {
        int row = i - offset;
        if (row < 0)
            continue;

        const char *name;
        if (g_list_mode == LIST_MODE_CHATS)
            name = g_chat_names[i];
        else if (g_list_mode == LIST_MODE_USERS)
            name = g_user_names[i];
        else
            name = g_member_names[i];

        if (i == g_list_selected && g_active == PANEL_LIST)
        {
            wattron(g_win_list_in, A_REVERSE);
            mvwprintw(g_win_list_in, row, 0, "%s", name);
            wattroff(g_win_list_in, A_REVERSE);
        }
        else if (i == g_list_selected)
        {
            wattron(g_win_list_in, A_BOLD);
            mvwprintw(g_win_list_in, row, 0, "> %s", name);
            wattroff(g_win_list_in, A_BOLD);
        }
        else if (g_list_mode == LIST_MODE_USERS && !g_user_has_msg[i])
        {
            wattron(g_win_list_in, A_DIM);
            mvwprintw(g_win_list_in, row, 0, "  %s", name);
            wattroff(g_win_list_in, A_DIM);
        }
        else
        {
            mvwprintw(g_win_list_in, row, 0, "  %s", name);
        }
    }

    wnoutrefresh(g_win_list);
    wnoutrefresh(g_win_list_in);
}

void draw_sys_bar(void)
{
    int cp = get_border_color(PANEL_SYS);
    wattron(g_win_sys, COLOR_PAIR(cp));
    box(g_win_sys, 0, 0);

    int bar_w;
    {
        int tmp;
        getmaxyx(g_win_sys, tmp, bar_w);
        (void)tmp;
    }

    mvwprintw(g_win_sys, 0, 2, " System ");
    wattroff(g_win_sys, COLOR_PAIR(cp));

    wattron(g_win_sys, COLOR_PAIR(CP_SYS));
    mvwprintw(g_win_sys, 1, 1, "%-*.*s", bar_w - 2, bar_w - 2, g_sys_msg);
    wattroff(g_win_sys, COLOR_PAIR(CP_SYS));

    if (g_active == PANEL_SYS)
    {
        curs_set(1);
        mvwprintw(g_win_sys, 1, 1, "> %-*.*s", bar_w - 4, bar_w - 4, g_sys_input);
        wmove(g_win_sys, 1, 3 + g_sys_input_len);
    }
    else
    {
        curs_set(0);
    }

    wnoutrefresh(g_win_sys);
}

void draw_notify(void)
{
    if (!g_notify_text[0])
        return;

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;

    int nw = (int)strlen(g_notify_text) + 4;
    if (nw > cols - 4)
        nw = cols - 4;
    int nx = (cols - nw) / 2;
    int ny = g_main_h - 4;
    if (ny < 0)
        ny = 0;

    WINDOW *w = newwin(3, nw, ny, nx);
    wattron(w, COLOR_PAIR(CP_NOTIFY));
    box(w, 0, 0);
    mvwprintw(w, 1, 2, "%.*s", nw - 4, g_notify_text);
    wattroff(w, COLOR_PAIR(CP_NOTIFY));
    wrefresh(w);
    delwin(w);
}

void draw_all(void)
{
    pthread_mutex_lock(&g_ui_mutex);
    draw_chat_panel();
    draw_list_panel();
    draw_sys_bar();
    doupdate();
    draw_notify();
    pthread_mutex_unlock(&g_ui_mutex);
}
