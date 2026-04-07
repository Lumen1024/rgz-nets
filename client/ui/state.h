#pragma once

#include <ncurses.h>
#include <pthread.h>
#include <protocol.h>
#include <ui.h>

// ─── Color pairs ─────────────────────────────────────────────────────────────
#define CP_DEFAULT 1
#define CP_SELECTED 2 // yellow border
#define CP_ACTIVE 3   // green border
#define CP_NOTIFY 4
#define CP_SYS 5 // system bar
#define CP_DIM 6 // grey text (users without messages)

// ─── Layout ──────────────────────────────────────────────────────────────────
#define SYS_BAR_H 3 // system bar height (border + 1 content row)

extern WINDOW *g_win_chat;
extern WINDOW *g_win_chat_in;
extern WINDOW *g_win_list;
extern WINDOW *g_win_list_in;
extern WINDOW *g_win_sys;

extern int g_rows, g_cols;
extern int g_left_w, g_right_w;
extern int g_main_h;

// ─── Chat panel state ─────────────────────────────────────────────────────────
#define MAX_MESSAGES 512
extern Message g_messages[MAX_MESSAGES];
extern int g_msg_count;
extern int g_msg_scroll;
extern char g_current_chat[MAX_ROUTE_LEN];
extern char g_input[MAX_TEXT_LEN];
extern int g_input_len;

// ─── Chat list state ──────────────────────────────────────────────────────────
#define MAX_CHATS 128
#define CHAT_ROUTE_LEN (MAX_ROUTE_LEN + 18)
extern char g_chat_names[MAX_CHATS][MAX_ROUTE_LEN];
extern int g_chat_count;

// ─── User list state ──────────────────────────────────────────────────────────
#define MAX_USERS 256
extern char g_user_names[MAX_USERS][MAX_LOGIN_LEN];
extern int g_user_has_msg[MAX_USERS];
extern int g_user_count;

// ─── Chat members state ───────────────────────────────────────────────────────
#define MAX_MEMBERS 256
extern char g_member_names[MAX_MEMBERS][MAX_LOGIN_LEN];
extern int g_member_count;

// ─── Right panel mode ─────────────────────────────────────────────────────────
extern ListMode g_list_mode;
extern int g_list_selected;

// ─── Focus/active state ───────────────────────────────────────────────────────
typedef enum
{
    PANEL_NONE = -1,
    PANEL_CHAT = 0,
    PANEL_LIST = 1,
    PANEL_SYS = 2
} Panel;
extern Panel g_focus;
extern Panel g_active;

// ─── System bar state ─────────────────────────────────────────────────────────
#define MAX_SYS_MSG 512
extern char g_sys_msg[MAX_SYS_MSG];
extern char g_sys_input[MAX_TEXT_LEN];
extern int g_sys_input_len;

typedef enum
{
    SYS_IDLE = 0,
    SYS_WAIT_CONFIRM
} SysState;
extern SysState g_sys_state;

// ─── Notification overlay ────────────────────────────────────────────────────
extern char g_notify_text[256];

// ─── Mutex for UI state ───────────────────────────────────────────────────────
extern pthread_mutex_t g_ui_mutex;
