#include <state.h>

// ncurses windows
WINDOW *g_win_chat = NULL;
WINDOW *g_win_chat_in = NULL;
WINDOW *g_win_list = NULL;
WINDOW *g_win_list_in = NULL;
WINDOW *g_win_sys = NULL;

// terminal dimensions
int g_rows = 0, g_cols = 0;
int g_left_w = 0, g_right_w = 0;
int g_main_h = 0;

// chat messages
Message g_messages[MAX_MESSAGES];
int g_msg_count = 0;
int g_msg_scroll = 0;
char g_current_chat[MAX_ROUTE_LEN] = {0};
char g_input[MAX_TEXT_LEN] = {0};
int g_input_len = 0;

// chat list
char g_chat_names[MAX_CHATS][MAX_ROUTE_LEN];
int g_chat_count = 0;

// user list
char g_user_names[MAX_USERS][MAX_LOGIN_LEN];
int g_user_has_msg[MAX_USERS];
int g_user_count = 0;

// member list (current chat)
char g_member_names[MAX_MEMBERS][MAX_LOGIN_LEN];
int g_member_count = 0;

// list panel state
ListMode g_list_mode = LIST_MODE_CHATS;
int g_list_selected = 0;

// focus and active panel
Panel g_focus = PANEL_CHAT;
Panel g_active = PANEL_NONE;

// system bar
char g_sys_msg[MAX_SYS_MSG] = {0};
time_t g_sys_msg_time = 0;
SysMsgType g_sys_msg_type = SYS_INFO;


// ui mutex
pthread_mutex_t g_ui_mutex = PTHREAD_MUTEX_INITIALIZER;

// session credentials
char g_token[MAX_TOKEN_LEN] = {0};
char g_login[MAX_LOGIN_LEN] = {0};
