#include <ui.h>
#include <actions.h>
#include <connection.h>
#include <request.h>
#include <response.h>
#include <protocol.h>

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <pthread.h>

// ─── Color pairs ─────────────────────────────────────────────────────────────
#define CP_DEFAULT   1
#define CP_SELECTED  2  // yellow border
#define CP_ACTIVE    3  // green border
#define CP_NOTIFY    4
#define CP_SYS       5  // system bar
#define CP_DIM       6  // grey text (users without messages)

// ─── Layout ──────────────────────────────────────────────────────────────────
// Left panel (70%): messages + input
// Right panel (30%): list (chats or users)
// Bottom bar (1 row): system messages + command input

#define SYS_BAR_H 3  // system bar height (border + 1 content row)

static WINDOW *g_win_chat      = NULL;
static WINDOW *g_win_chat_in   = NULL;
static WINDOW *g_win_list      = NULL;
static WINDOW *g_win_list_in   = NULL;
static WINDOW *g_win_sys       = NULL; // system bar window

static int g_rows = 0, g_cols = 0;
static int g_left_w = 0, g_right_w = 0;
static int g_main_h = 0; // height of main area (rows - SYS_BAR_H)

// ─── Chat panel state ─────────────────────────────────────────────────────────
#define MAX_MESSAGES 512
static Message  g_messages[MAX_MESSAGES];
static int      g_msg_count   = 0;
static int      g_msg_scroll  = 0;
static char     g_current_chat[MAX_ROUTE_LEN] = {0};
static char     g_input[MAX_TEXT_LEN]         = {0};
static int      g_input_len   = 0;

// ─── Chat list state ──────────────────────────────────────────────────────────
#define MAX_CHATS 128
#define CHAT_ROUTE_LEN (MAX_ROUTE_LEN + 18)
static char  g_chat_names[MAX_CHATS][MAX_ROUTE_LEN];
static int   g_chat_count    = 0;

// ─── User list state ──────────────────────────────────────────────────────────
#define MAX_USERS 256
static char g_user_names[MAX_USERS][MAX_LOGIN_LEN];
static int  g_user_has_msg[MAX_USERS]; // 1 = has dialog, 0 = no messages
static int  g_user_count = 0;

// ─── Right panel mode ─────────────────────────────────────────────────────────
static ListMode g_list_mode    = LIST_MODE_CHATS;
static int      g_list_selected = 0;

// ─── Focus/active state ───────────────────────────────────────────────────────
typedef enum { PANEL_NONE = -1, PANEL_CHAT = 0, PANEL_LIST = 1, PANEL_SYS = 2 } Panel;
static Panel g_focus  = PANEL_CHAT;
static Panel g_active = PANEL_NONE;

// ─── System bar state ─────────────────────────────────────────────────────────
#define MAX_SYS_MSG 512
static char g_sys_msg[MAX_SYS_MSG] = {0}; // last system message
static char g_sys_input[MAX_TEXT_LEN] = {0};
static int  g_sys_input_len = 0;
// When sys bar is collecting input for a specific operation:
typedef enum {
    SYS_IDLE = 0,
    SYS_WAIT_CONFIRM  // awaiting y/n confirmation
} SysState;
static SysState g_sys_state = SYS_IDLE;

// ─── Notification overlay ────────────────────────────────────────────────────
static char g_notify_text[256] = {0};

// ─── Mutex for UI state (modified from reader thread) ─────────────────────────
static pthread_mutex_t g_ui_mutex = PTHREAD_MUTEX_INITIALIZER;

// ─── Forward declarations ────────────────────────────────────────────────────
static void draw_all(void);
static void draw_chat_panel(void);
static void draw_list_panel(void);
static void draw_sys_bar(void);
static void draw_notify(void);
static int  border_color(Panel panel);
static void load_chat_messages(const char *route);
static void load_chat_list(void);
static void load_user_list(void);
static void list_current_count(int *count);
static const char *list_current_name(int idx);
static void handle_sys_input(void);
static void handle_command(const char *cmd);
static void open_selected_item(void);

// ─── Init / destroy ──────────────────────────────────────────────────────────

void ui_init(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    start_color();
    init_pair(CP_DEFAULT,  COLOR_WHITE,  COLOR_BLACK);
    init_pair(CP_SELECTED, COLOR_YELLOW, COLOR_BLACK);
    init_pair(CP_ACTIVE,   COLOR_GREEN,  COLOR_BLACK);
    init_pair(CP_NOTIFY,   COLOR_BLACK,  COLOR_YELLOW);
    init_pair(CP_SYS,      COLOR_CYAN,   COLOR_BLACK);
    init_pair(CP_DIM,      COLOR_WHITE,  COLOR_BLACK); // will use A_DIM

    getmaxyx(stdscr, g_rows, g_cols);
    g_main_h  = g_rows - SYS_BAR_H;
    g_left_w  = (g_cols * 7) / 10;
    g_right_w = g_cols - g_left_w;

    // Main panels occupy top (g_main_h) rows
    g_win_chat    = newwin(g_main_h, g_left_w,  0, 0);
    g_win_list    = newwin(g_main_h, g_right_w, 0, g_left_w);

    g_win_chat_in = newwin(g_main_h - 2, g_left_w  - 2, 1, 1);
    g_win_list_in = newwin(g_main_h - 2, g_right_w - 2, 1, g_left_w + 1);

    // System bar at the bottom
    g_win_sys = newwin(SYS_BAR_H, g_cols, g_main_h, 0);

    scrollok(g_win_chat_in, FALSE);
    scrollok(g_win_list_in, FALSE);
}

void ui_destroy(void) {
    if (g_win_chat_in) { delwin(g_win_chat_in); g_win_chat_in = NULL; }
    if (g_win_list_in) { delwin(g_win_list_in); g_win_list_in = NULL; }
    if (g_win_chat)    { delwin(g_win_chat);    g_win_chat    = NULL; }
    if (g_win_list)    { delwin(g_win_list);    g_win_list    = NULL; }
    if (g_win_sys)     { delwin(g_win_sys);     g_win_sys     = NULL; }
    endwin();
}

// ─── Prompt helpers (run before ui_init, plain terminal) ─────────────────────

static void read_line(const char *prompt, char *out, int maxlen, int hidden) {
    printf("%s", prompt);
    fflush(stdout);

    if (hidden) {
        struct termios old, noecho;
        tcgetattr(STDIN_FILENO, &old);
        noecho = old;
        noecho.c_lflag &= ~(tcflag_t)ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &noecho);

        if (fgets(out, maxlen, stdin)) {
            out[strcspn(out, "\n")] = '\0';
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &old);
        printf("\n");
    } else {
        if (fgets(out, maxlen, stdin)) {
            out[strcspn(out, "\n")] = '\0';
        }
    }
}

int ui_prompt_server(char *host_out, int *port_out) {
    char buf[128];
    read_line("Server address (host:port): ", buf, sizeof(buf), 0);

    char *colon = strrchr(buf, ':');
    if (colon) {
        *colon = '\0';
        strncpy(host_out, buf, 63);
        *port_out = atoi(colon + 1);
    } else {
        strncpy(host_out, buf, 63);
        *port_out = 8080;
    }
    return 0;
}

int ui_prompt_login(char *login_out, char *password_out) {
    read_line("Login: ",    login_out,    64, 0);
    read_line("Password: ", password_out, 64, 1);
    return 0;
}

int ui_prompt_register(char *login_out, char *password_out) {
    read_line("New login: ",    login_out,    64, 0);
    read_line("New password: ", password_out, 64, 1);
    return 0;
}

// ─── Public state setters (called from actions / reader) ─────────────────────

void ui_set_chat(const char *chat_name, Message *msgs, int count) {
    pthread_mutex_lock(&g_ui_mutex);
    strncpy(g_current_chat, chat_name, MAX_ROUTE_LEN - 1);
    g_msg_count  = count < MAX_MESSAGES ? count : MAX_MESSAGES;
    g_msg_scroll = 0;
    for (int i = 0; i < g_msg_count; i++) {
        g_messages[i] = msgs[i];
    }
    pthread_mutex_unlock(&g_ui_mutex);
    draw_all();
}

void ui_set_chat_list(char **names, int count) {
    pthread_mutex_lock(&g_ui_mutex);
    g_chat_count = count < MAX_CHATS ? count : MAX_CHATS;
    for (int i = 0; i < g_chat_count; i++) {
        strncpy(g_chat_names[i], names[i], MAX_ROUTE_LEN - 1);
    }
    g_list_selected = 0;
    pthread_mutex_unlock(&g_ui_mutex);
    draw_all();
}

void ui_set_user_list(char **names, int *has_messages, int count) {
    pthread_mutex_lock(&g_ui_mutex);
    g_user_count = count < MAX_USERS ? count : MAX_USERS;
    for (int i = 0; i < g_user_count; i++) {
        strncpy(g_user_names[i], names[i], MAX_LOGIN_LEN - 1);
        g_user_has_msg[i] = has_messages[i];
    }
    g_list_selected = 0;
    pthread_mutex_unlock(&g_ui_mutex);
    draw_all();
}

void ui_append_message(Message *msg) {
    pthread_mutex_lock(&g_ui_mutex);
    if (g_msg_count < MAX_MESSAGES) {
        g_messages[g_msg_count++] = *msg;
    }
    pthread_mutex_unlock(&g_ui_mutex);
    draw_chat_panel();
    wrefresh(g_win_chat);
    wrefresh(g_win_chat_in);
}

void ui_notify(const char *text) {
    pthread_mutex_lock(&g_ui_mutex);
    strncpy(g_notify_text, text, sizeof(g_notify_text) - 1);
    pthread_mutex_unlock(&g_ui_mutex);
    draw_all();
}

void ui_sys(const char *text) {
    pthread_mutex_lock(&g_ui_mutex);
    strncpy(g_sys_msg, text, MAX_SYS_MSG - 1);
    pthread_mutex_unlock(&g_ui_mutex);
    draw_sys_bar();
    wnoutrefresh(g_win_sys);
    doupdate();
}

// ─── Drawing ─────────────────────────────────────────────────────────────────

static int border_color(Panel panel) {
    if (g_active == panel)  return CP_ACTIVE;
    if (g_focus  == panel)  return CP_SELECTED;
    return CP_DEFAULT;
}

static void draw_chat_panel(void) {
    int cp = border_color(PANEL_CHAT);
    wattron(g_win_chat, COLOR_PAIR(cp));
    box(g_win_chat, 0, 0);

    const char *title = g_current_chat[0] ? g_current_chat : "[ no chat ]";
    mvwprintw(g_win_chat, 0, 2, " %s ", title);
    wattroff(g_win_chat, COLOR_PAIR(cp));

    int inner_h, inner_w;
    getmaxyx(g_win_chat_in, inner_h, inner_w);
    (void)inner_w;

    int msg_h = inner_h - 2; // leave 1 line for input + 1 separator

    werase(g_win_chat_in);

    int visible = msg_h;
    int total   = g_msg_count;
    int start   = total - visible - g_msg_scroll;
    if (start < 0) start = 0;
    int end     = start + visible;
    if (end > total) end = total;

    for (int i = start; i < end; i++) {
        int row = i - start;
        mvwprintw(g_win_chat_in, row, 0, "[%s] %s",
            g_messages[i].login, g_messages[i].text);
    }

    mvwhline(g_win_chat_in, msg_h, 0, ACS_HLINE, inner_w);

    if (g_active == PANEL_CHAT) {
        curs_set(1);
        mvwprintw(g_win_chat_in, msg_h + 1, 0, "> %s", g_input);
        wmove(g_win_chat_in, msg_h + 1, 2 + g_input_len);
    } else {
        curs_set(0);
        mvwprintw(g_win_chat_in, msg_h + 1, 0, "> ");
    }

    wnoutrefresh(g_win_chat);
    wnoutrefresh(g_win_chat_in);
}

static void draw_list_panel(void) {
    int cp = border_color(PANEL_LIST);
    wattron(g_win_list, COLOR_PAIR(cp));
    box(g_win_list, 0, 0);

    // Title shows current mode + hint for switching
    if (g_list_mode == LIST_MODE_CHATS) {
        mvwprintw(g_win_list, 0, 2, " Chats [</>] ");
    } else {
        mvwprintw(g_win_list, 0, 2, " Users [</>] ");
    }
    wattroff(g_win_list, COLOR_PAIR(cp));

    int inner_h, inner_w;
    getmaxyx(g_win_list_in, inner_h, inner_w);
    (void)inner_w;

    werase(g_win_list_in);

    int count = 0;
    list_current_count(&count);

    int offset = 0;
    if (g_list_selected >= inner_h) {
        offset = g_list_selected - inner_h + 1;
    }

    for (int i = 0; i < count && (i - offset) < inner_h; i++) {
        int row = i - offset;
        if (row < 0) continue;

        const char *name = list_current_name(i);

        if (i == g_list_selected && g_active == PANEL_LIST) {
            wattron(g_win_list_in, A_REVERSE);
            mvwprintw(g_win_list_in, row, 0, "%s", name);
            wattroff(g_win_list_in, A_REVERSE);
        } else if (i == g_list_selected) {
            wattron(g_win_list_in, A_BOLD);
            mvwprintw(g_win_list_in, row, 0, "> %s", name);
            wattroff(g_win_list_in, A_BOLD);
        } else if (g_list_mode == LIST_MODE_USERS && !g_user_has_msg[i]) {
            // Grey out users without messages
            wattron(g_win_list_in, A_DIM);
            mvwprintw(g_win_list_in, row, 0, "  %s", name);
            wattroff(g_win_list_in, A_DIM);
        } else {
            mvwprintw(g_win_list_in, row, 0, "  %s", name);
        }
    }

    wnoutrefresh(g_win_list);
    wnoutrefresh(g_win_list_in);
}

static void draw_sys_bar(void) {
    int cp = border_color(PANEL_SYS);
    wattron(g_win_sys, COLOR_PAIR(cp));
    box(g_win_sys, 0, 0);

    int bar_w;
    {
        int tmp;
        getmaxyx(g_win_sys, tmp, bar_w);
        (void)tmp;
    }

    // Show mode hint in title
    mvwprintw(g_win_sys, 0, 2, " System ");
    wattroff(g_win_sys, COLOR_PAIR(cp));

    // Line 1 inside: system message (last status)
    wattron(g_win_sys, COLOR_PAIR(CP_SYS));
    mvwprintw(g_win_sys, 1, 1, "%-*.*s", bar_w - 2, bar_w - 2, g_sys_msg);
    wattroff(g_win_sys, COLOR_PAIR(CP_SYS));

    // Input line is drawn when sys bar is active
    if (g_active == PANEL_SYS) {
        curs_set(1);
        // Show prompt in last row of sys bar (row 2 if SYS_BAR_H=3, but we use line 1 for msg and input inline)
        // Since SYS_BAR_H=3: row0=border, row1=content, row2=border
        // We'll reuse row1: left half = status, replaced by input when active
        mvwprintw(g_win_sys, 1, 1, "> %-*.*s", bar_w - 4, bar_w - 4, g_sys_input);
        wmove(g_win_sys, 1, 3 + g_sys_input_len);
    } else {
        curs_set(0);
    }

    wnoutrefresh(g_win_sys);
}

static void draw_notify(void) {
    if (!g_notify_text[0]) return;

    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    (void)rows;

    int nw = (int)strlen(g_notify_text) + 4;
    if (nw > cols - 4) nw = cols - 4;
    int nx = (cols - nw) / 2;
    int ny = g_main_h - 4;
    if (ny < 0) ny = 0;

    WINDOW *w = newwin(3, nw, ny, nx);
    wattron(w, COLOR_PAIR(CP_NOTIFY));
    box(w, 0, 0);
    mvwprintw(w, 1, 2, "%.*s", nw - 4, g_notify_text);
    wattroff(w, COLOR_PAIR(CP_NOTIFY));
    wrefresh(w);
    delwin(w);
}

static void draw_all(void) {
    pthread_mutex_lock(&g_ui_mutex);
    draw_chat_panel();
    draw_list_panel();
    draw_sys_bar();
    doupdate();
    draw_notify();
    pthread_mutex_unlock(&g_ui_mutex);
}

// ─── Helper: current list item access ────────────────────────────────────────

static void list_current_count(int *count) {
    if (g_list_mode == LIST_MODE_CHATS) {
        *count = g_chat_count;
    } else {
        *count = g_user_count;
    }
}

static const char *list_current_name(int idx) {
    if (g_list_mode == LIST_MODE_CHATS) {
        return g_chat_names[idx];
    } else {
        return g_user_names[idx];
    }
}

// ─── Helper: open selected item in list ──────────────────────────────────────

static void open_selected_item(void) {
    int count = 0;
    list_current_count(&count);
    if (count == 0) return;

    if (g_list_mode == LIST_MODE_CHATS) {
        char route[CHAT_ROUTE_LEN];
        snprintf(route, sizeof(route), "/chats/%s/messages",
            g_chat_names[g_list_selected]);
        g_active = PANEL_NONE;
        g_focus  = PANEL_CHAT;
        load_chat_messages(route);
    } else {
        // Open private dialog with selected user
        char route[MAX_ROUTE_LEN];
        snprintf(route, sizeof(route), "/users/%s/messages",
            g_user_names[g_list_selected]);
        g_active = PANEL_NONE;
        g_focus  = PANEL_CHAT;
        load_chat_messages(route);
    }
}

// ─── Helper: open a chat/dialog by route ─────────────────────────────────────

static void load_chat_messages(const char *route) {
    Request req;
    req.kind    = MSG_REQUEST;
    req.type    = GET;
    req.route   = (char *)route;
    req.token   = NULL;
    req.content = NULL;

    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content) {
        free_response(&res);
        return;
    }

    Message msgs[MAX_MESSAGES];
    int count = 0;
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < MAX_MESSAGES; i++) {
        cJSON *item = cJSON_GetArrayItem(res.content, i);
        cJSON *l    = cJSON_GetObjectItemCaseSensitive(item, "login");
        cJSON *t    = cJSON_GetObjectItemCaseSensitive(item, "text");
        cJSON *ts   = cJSON_GetObjectItemCaseSensitive(item, "timestamp");

        memset(&msgs[count], 0, sizeof(Message));
        if (cJSON_IsString(l))  strncpy(msgs[count].login,     l->valuestring,  MAX_LOGIN_LEN     - 1);
        if (cJSON_IsString(t))  strncpy(msgs[count].text,      t->valuestring,  MAX_TEXT_LEN      - 1);
        if (cJSON_IsString(ts)) strncpy(msgs[count].timestamp, ts->valuestring, MAX_TIMESTAMP_LEN - 1);
        count++;
    }

    free_response(&res);
    ui_set_chat(route, msgs, count);
}

static void load_chat_list(void) {
    Request req;
    req.kind    = MSG_REQUEST;
    req.type    = GET;
    req.route   = "/chats";
    req.token   = NULL;
    req.content = NULL;

    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content) {
        free_response(&res);
        return;
    }

    char *names[MAX_CHATS];
    int count = 0;
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < MAX_CHATS; i++) {
        cJSON *item = cJSON_GetArrayItem(res.content, i);
        if (cJSON_IsString(item)) {
            names[count++] = item->valuestring;
        }
    }

    ui_set_chat_list(names, count);
    free_response(&res);
}

static void load_user_list(void) {
    Request req;
    req.kind    = MSG_REQUEST;
    req.type    = GET;
    req.route   = "/users";
    req.token   = NULL;
    req.content = NULL;

    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content) {
        free_response(&res);
        return;
    }

    char *names[MAX_USERS];
    int  has_msg[MAX_USERS];
    int count = 0;
    int arr_size = cJSON_GetArraySize(res.content);
    for (int i = 0; i < arr_size && count < MAX_USERS; i++) {
        cJSON *item = cJSON_GetArrayItem(res.content, i);
        if (cJSON_IsString(item)) {
            names[count]   = item->valuestring;
            has_msg[count] = 0; // default: no messages; server may return richer data later
            count++;
        }
    }

    ui_set_user_list(names, has_msg, count);
    free_response(&res);
}

// ─── Command processing ───────────────────────────────────────────────────────

// Extract "chat name" from current route, e.g. "/chats/general/messages" -> "general"
static int current_chat_name(char *out, int maxlen) {
    // Routes: /chats/{name}/messages  or  /users/{login}/messages
    if (strncmp(g_current_chat, "/chats/", 7) == 0) {
        const char *p = g_current_chat + 7;
        const char *slash = strchr(p, '/');
        int len = slash ? (int)(slash - p) : (int)strlen(p);
        if (len >= maxlen) return 0;
        strncpy(out, p, len);
        out[len] = '\0';
        return 1;
    }
    return 0;
}

static int is_private_chat(void) {
    return strncmp(g_current_chat, "/users/", 7) == 0;
}

static void handle_command(const char *cmd) {
    char arg[MAX_SYS_MSG];
    char chat[MAX_ROUTE_LEN];

    // /create <chatname>
    if (strncmp(cmd, "/create ", 8) == 0) {
        const char *name = cmd + 8;
        if (name[0] == '\0') {
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
        // Refresh chat list after a brief yield
        load_chat_list();
        return;
    }

    // /add <username>  — add user to current chat
    if (strncmp(cmd, "/add ", 5) == 0) {
        const char *login = cmd + 5;
        if (login[0] == '\0') {
            ui_sys("Usage: /add <username>");
            return;
        }
        if (!current_chat_name(chat, sizeof(chat))) {
            ui_sys("Error: not in a group chat");
            return;
        }
        ChatUserArgs *a = malloc(sizeof(ChatUserArgs));
        strncpy(a->chat,  chat,  sizeof(a->chat)  - 1);
        strncpy(a->login, login, sizeof(a->login) - 1);
        pthread_t tid;
        pthread_create(&tid, NULL, action_add_chat_user, a);
        pthread_detach(tid);
        snprintf(arg, sizeof(arg), "Adding '%s' to '%s'...", login, chat);
        ui_sys(arg);
        return;
    }

    // /delete <username>  — remove user from current chat (creator only)
    if (strncmp(cmd, "/delete ", 8) == 0) {
        const char *login = cmd + 8;
        if (login[0] == '\0') {
            ui_sys("Usage: /delete <username>");
            return;
        }
        if (!current_chat_name(chat, sizeof(chat))) {
            ui_sys("Error: not in a group chat");
            return;
        }
        ChatUserArgs *a = malloc(sizeof(ChatUserArgs));
        strncpy(a->chat,  chat,  sizeof(a->chat)  - 1);
        strncpy(a->login, login, sizeof(a->login) - 1);
        pthread_t tid;
        pthread_create(&tid, NULL, action_remove_chat_user, a);
        pthread_detach(tid);
        snprintf(arg, sizeof(arg), "Removing '%s' from '%s'...", login, chat);
        ui_sys(arg);
        return;
    }

    // /leave  — leave current group chat
    if (strcmp(cmd, "/leave") == 0) {
        if (is_private_chat()) {
            ui_sys("Error: cannot leave a private dialog");
            return;
        }
        if (!current_chat_name(chat, sizeof(chat))) {
            ui_sys("Error: not in a group chat");
            return;
        }
        LeaveChatArgs *a = malloc(sizeof(LeaveChatArgs));
        strncpy(a->chat, chat, sizeof(a->chat) - 1);
        pthread_t tid;
        pthread_create(&tid, NULL, action_leave_chat, a);
        pthread_detach(tid);
        // Clear chat view and reload list
        g_current_chat[0] = '\0';
        g_msg_count = 0;
        load_chat_list();
        ui_sys("Left the chat.");
        return;
    }

    snprintf(arg, sizeof(arg), "Unknown command: %s", cmd);
    ui_sys(arg);
}

// Called when user presses Enter in the sys bar
static void handle_sys_input(void) {
    if (g_sys_input_len == 0) return;

    if (g_sys_input[0] == '/') {
        handle_command(g_sys_input);
    } else {
        // Plain system input (future use)
        ui_sys(g_sys_input);
    }

    g_sys_input[0]   = '\0';
    g_sys_input_len  = 0;
    g_sys_state      = SYS_IDLE;
}

// ─── Main event loop ─────────────────────────────────────────────────────────

void ui_run(void) {
    load_chat_list();
    load_user_list();
    draw_all();

    int ch;
    while ((ch = getch()) != 'q') {
        // Dismiss notification on any key
        if (g_notify_text[0]) {
            g_notify_text[0] = '\0';
            draw_all();
            continue;
        }

        // ── System bar active ────────────────────────────────────────────────
        if (g_active == PANEL_SYS) {
            switch (ch) {
                case '\n':
                case KEY_ENTER:
                    handle_sys_input();
                    g_active = PANEL_NONE;
                    break;
                case KEY_BACKSPACE:
                case 127:
                    if (g_sys_input_len > 0) {
                        g_sys_input[--g_sys_input_len] = '\0';
                    }
                    break;
                case 27: // Escape
                    g_sys_input[0]  = '\0';
                    g_sys_input_len = 0;
                    g_sys_state     = SYS_IDLE;
                    g_active        = PANEL_NONE;
                    break;
                default:
                    if (ch >= 32 && ch < 256 && g_sys_input_len < MAX_TEXT_LEN - 1) {
                        g_sys_input[g_sys_input_len++] = (char)ch;
                        g_sys_input[g_sys_input_len]   = '\0';
                    }
                    break;
            }
            draw_all();
            continue;
        }

        // ── No panel active: navigation ──────────────────────────────────────
        if (g_active == PANEL_NONE) {
            switch (ch) {
                case KEY_LEFT:
                    if (g_focus == PANEL_LIST) {
                        g_focus = PANEL_CHAT;
                    } else if (g_focus == PANEL_SYS) {
                        g_focus = PANEL_CHAT;
                    }
                    break;
                case KEY_RIGHT:
                    if (g_focus == PANEL_CHAT) {
                        g_focus = PANEL_LIST;
                    } else if (g_focus == PANEL_SYS) {
                        g_focus = PANEL_LIST;
                    }
                    break;
                case KEY_DOWN:
                    if (g_focus != PANEL_SYS) g_focus = PANEL_SYS;
                    break;
                case KEY_UP:
                    if (g_focus == PANEL_SYS) g_focus = PANEL_CHAT;
                    break;
                case '\n':
                case KEY_ENTER:
                    g_active = g_focus;
                    // When entering sys bar, clear old input
                    if (g_active == PANEL_SYS) {
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

        // ── List panel active ────────────────────────────────────────────────
        if (g_active == PANEL_LIST) {
            int count = 0;
            list_current_count(&count);
            switch (ch) {
                case KEY_UP:
                    if (g_list_selected > 0) g_list_selected--;
                    break;
                case KEY_DOWN:
                    if (g_list_selected < count - 1) g_list_selected++;
                    break;
                case KEY_LEFT:
                    // Switch mode to chats
                    g_list_mode     = LIST_MODE_CHATS;
                    g_list_selected = 0;
                    load_chat_list();
                    break;
                case KEY_RIGHT:
                    // Switch mode to users
                    g_list_mode     = LIST_MODE_USERS;
                    g_list_selected = 0;
                    load_user_list();
                    break;
                case '\n':
                case KEY_ENTER:
                    open_selected_item();
                    break;
                case 27: // Escape
                    g_active = PANEL_NONE;
                    break;
                default:
                    break;
            }
            draw_all();
            continue;
        }

        // ── Chat panel active ────────────────────────────────────────────────
        if (g_active == PANEL_CHAT) {
            switch (ch) {
                case KEY_UP:
                    if (g_msg_scroll < g_msg_count) g_msg_scroll++;
                    break;
                case KEY_DOWN:
                    if (g_msg_scroll > 0) g_msg_scroll--;
                    break;
                case '\n':
                case KEY_ENTER:
                    if (g_input_len > 0 && g_current_chat[0]) {
                        if (g_input[0] == '/') {
                            // Command entered in chat input
                            handle_command(g_input);
                        } else {
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
                    if (g_input_len > 0) {
                        g_input[--g_input_len] = '\0';
                    }
                    break;
                case 27: // Escape
                    g_active = PANEL_NONE;
                    break;
                default:
                    if (ch >= 32 && ch < 256 && g_input_len < MAX_TEXT_LEN - 1) {
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
