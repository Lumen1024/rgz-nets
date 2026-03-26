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
#define CP_DEFAULT  1
#define CP_SELECTED 2  // yellow border
#define CP_ACTIVE   3  // green border
#define CP_NOTIFY   4

// ─── Layout ──────────────────────────────────────────────────────────────────
// Left panel: chat view   Right panel: chat list
// Ratio: 70% / 30%

static WINDOW *g_win_chat      = NULL; // left border window
static WINDOW *g_win_chat_in   = NULL; // inner content of left panel
static WINDOW *g_win_list      = NULL; // right border window
static WINDOW *g_win_list_in   = NULL; // inner content of right panel

static int g_rows = 0, g_cols = 0;
static int g_left_w = 0, g_right_w = 0;

// ─── Chat panel state ─────────────────────────────────────────────────────────
#define MAX_MESSAGES 512
static Message  g_messages[MAX_MESSAGES];
static int      g_msg_count   = 0;
static int      g_msg_scroll  = 0; // index of first visible message (from bottom)
static char     g_current_chat[MAX_ROUTE_LEN] = {0};
static char     g_input[MAX_TEXT_LEN]         = {0};
static int      g_input_len   = 0;

// ─── Chat list state ──────────────────────────────────────────────────────────
#define MAX_CHATS 128
#define CHAT_ROUTE_LEN (MAX_ROUTE_LEN + 18) // "/chats/" + name + "/messages"
static char  g_chat_names[MAX_CHATS][MAX_ROUTE_LEN];
static int   g_chat_count    = 0;
static int   g_list_selected = 0; // cursor in list panel

// ─── Focus/active state ───────────────────────────────────────────────────────
// focus: which panel is highlighted (keyboard cursor between panels)
// active: which panel is currently receiving input
typedef enum { PANEL_NONE = -1, PANEL_CHAT = 0, PANEL_LIST = 1 } Panel;
static Panel g_focus  = PANEL_CHAT;
static Panel g_active = PANEL_NONE;

// ─── Mutex for UI state (modified from reader thread) ─────────────────────────
static pthread_mutex_t g_ui_mutex = PTHREAD_MUTEX_INITIALIZER;

// ─── Notification overlay ────────────────────────────────────────────────────
static char g_notify_text[256] = {0};

// ─── Forward declarations ────────────────────────────────────────────────────
static void draw_all(void);
static void draw_chat_panel(void);
static void draw_list_panel(void);
static void draw_notify(void);
static int  border_color(Panel panel);
static void load_chat_messages(const char *route);
static void load_chat_list(void);

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

    getmaxyx(stdscr, g_rows, g_cols);
    g_left_w  = (g_cols * 7) / 10;
    g_right_w = g_cols - g_left_w;

    // Border windows (full height)
    g_win_chat    = newwin(g_rows, g_left_w,  0, 0);
    g_win_list    = newwin(g_rows, g_right_w, 0, g_left_w);

    // Inner windows (inside the border: -2 rows for top/bottom border, -2 cols)
    g_win_chat_in = newwin(g_rows - 2, g_left_w  - 2, 1, 1);
    g_win_list_in = newwin(g_rows - 2, g_right_w - 2, 1, g_left_w + 1);

    scrollok(g_win_chat_in, FALSE);
    scrollok(g_win_list_in, FALSE);
}

void ui_destroy(void) {
    if (g_win_chat_in) { delwin(g_win_chat_in); g_win_chat_in = NULL; }
    if (g_win_list_in) { delwin(g_win_list_in); g_win_list_in = NULL; }
    if (g_win_chat)    { delwin(g_win_chat);    g_win_chat    = NULL; }
    if (g_win_list)    { delwin(g_win_list);    g_win_list    = NULL; }
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

    // Title
    const char *title = g_current_chat[0] ? g_current_chat : "[ no chat ]";
    mvwprintw(g_win_chat, 0, 2, " %s ", title);
    wattroff(g_win_chat, COLOR_PAIR(cp));

    // Inner area: messages above, input line at bottom
    int inner_h, inner_w;
    getmaxyx(g_win_chat_in, inner_h, inner_w);
    (void)inner_w;

    int msg_h = inner_h - 2; // leave 1 line for input + 1 separator

    werase(g_win_chat_in);

    // Draw messages (bottom-aligned, scroll offset from bottom)
    int visible  = msg_h;
    int total    = g_msg_count;
    int start    = total - visible - g_msg_scroll;
    if (start < 0) start = 0;
    int end      = start + visible;
    if (end > total) end = total;

    for (int i = start; i < end; i++) {
        int row = i - start;
        mvwprintw(g_win_chat_in, row, 0, "[%s] %s",
            g_messages[i].login, g_messages[i].text);
    }

    // Separator line
    mvwhline(g_win_chat_in, msg_h, 0, ACS_HLINE, inner_w);

    // Input line
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
    mvwprintw(g_win_list, 0, 2, " Chats ");
    wattroff(g_win_list, COLOR_PAIR(cp));

    int inner_h, inner_w;
    getmaxyx(g_win_list_in, inner_h, inner_w);
    (void)inner_w;

    werase(g_win_list_in);

    // Scroll list so selected item is always visible
    int offset = 0;
    if (g_list_selected >= inner_h) {
        offset = g_list_selected - inner_h + 1;
    }

    for (int i = 0; i < g_chat_count && (i - offset) < inner_h; i++) {
        int row = i - offset;
        if (row < 0) continue;
        if (i == g_list_selected && g_active == PANEL_LIST) {
            wattron(g_win_list_in, A_REVERSE);
            mvwprintw(g_win_list_in, row, 0, "%s", g_chat_names[i]);
            wattroff(g_win_list_in, A_REVERSE);
        } else if (i == g_list_selected) {
            wattron(g_win_list_in, A_BOLD);
            mvwprintw(g_win_list_in, row, 0, "> %s", g_chat_names[i]);
            wattroff(g_win_list_in, A_BOLD);
        } else {
            mvwprintw(g_win_list_in, row, 0, "  %s", g_chat_names[i]);
        }
    }

    wnoutrefresh(g_win_list);
    wnoutrefresh(g_win_list_in);
}

static void draw_notify(void) {
    if (!g_notify_text[0]) return;

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int nw = (int)strlen(g_notify_text) + 4;
    if (nw > cols - 4) nw = cols - 4;
    int nx = (cols - nw) / 2;
    int ny = rows - 3;

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
    doupdate();
    draw_notify();
    pthread_mutex_unlock(&g_ui_mutex);
}

// ─── Helper: open a chat by index ─────────────────────────────────────────────

static void load_chat_messages(const char *route) {
    Request req;
    req.kind    = MSG_REQUEST;
    req.type    = GET;
    req.route   = (char *)route;
    req.token   = NULL; // connection.c fills token via send_and_wait -> send_request
    req.content = NULL;

    Response res = send_and_wait(req);
    if (res.code != ERR_OK || !res.content) {
        free_response(&res);
        return;
    }

    // content is array of {login, text, timestamp}
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

// ─── Main event loop ─────────────────────────────────────────────────────────

void ui_run(void) {
    load_chat_list();
    draw_all();

    int ch;
    while ((ch = getch()) != 'q') {
        // Dismiss notification on any key
        if (g_notify_text[0]) {
            g_notify_text[0] = '\0';
            draw_all();
            continue;
        }

        if (g_active == PANEL_NONE) {
            // Navigation between panels
            switch (ch) {
                case KEY_LEFT:
                    g_focus = PANEL_CHAT;
                    break;
                case KEY_RIGHT:
                    g_focus = PANEL_LIST;
                    break;
                case '\n':
                case KEY_ENTER:
                    g_active = g_focus;
                    break;
                default:
                    break;
            }
            draw_all();
            continue;
        }

        if (g_active == PANEL_LIST) {
            switch (ch) {
                case KEY_UP:
                    if (g_list_selected > 0) g_list_selected--;
                    break;
                case KEY_DOWN:
                    if (g_list_selected < g_chat_count - 1) g_list_selected++;
                    break;
                case '\n':
                case KEY_ENTER:
                    if (g_chat_count > 0) {
                        char route[CHAT_ROUTE_LEN];
                        snprintf(route, sizeof(route), "/chats/%s/messages",
                            g_chat_names[g_list_selected]);
                        g_active = PANEL_NONE;
                        g_focus  = PANEL_CHAT;
                        load_chat_messages(route);
                    }
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

        if (g_active == PANEL_CHAT) {
            switch (ch) {
                case KEY_UP:
                    // Scroll history up (show older messages)
                    if (g_msg_scroll < g_msg_count) g_msg_scroll++;
                    break;
                case KEY_DOWN:
                    if (g_msg_scroll > 0) g_msg_scroll--;
                    break;
                case '\n':
                case KEY_ENTER:
                    if (g_input_len > 0 && g_current_chat[0]) {
                        // Send message in a detached thread
                        SendMessageArgs *a = malloc(sizeof(SendMessageArgs));
                        strncpy(a->chat, g_current_chat, sizeof(a->chat) - 1);
                        strncpy(a->text, g_input, sizeof(a->text) - 1);
                        pthread_t tid;
                        pthread_create(&tid, NULL, action_send_message, a);
                        pthread_detach(tid);

                        g_input[0]  = '\0';
                        g_input_len = 0;
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
