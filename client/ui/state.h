#pragma once

#include <ncurses.h>
#include <pthread.h>
#include <time.h>
#include <protocol.h>
#include <config.h>
#include <api/model.h>


extern WINDOW *g_win_chat;
extern WINDOW *g_win_chat_in;
extern WINDOW *g_win_list;
extern WINDOW *g_win_list_in;
extern WINDOW *g_win_sys;

extern int g_rows, g_cols;
extern int g_left_w, g_right_w;
extern int g_main_h;

extern Message g_messages[MAX_MESSAGES];
extern int g_msg_count;
extern int g_msg_scroll;
extern char g_current_chat[MAX_ROUTE_LEN];
extern char g_input[MAX_TEXT_LEN];
extern int g_input_len;

extern char g_chat_names[MAX_CHATS][MAX_ROUTE_LEN];
extern int g_chat_count;

extern char g_user_names[MAX_USERS][MAX_LOGIN_LEN];
extern int g_user_has_msg[MAX_USERS];
extern int g_user_count;

extern char g_member_names[MAX_MEMBERS][MAX_LOGIN_LEN];
extern int g_member_count;

extern ListMode g_list_mode;
extern int g_list_selected;

extern Panel g_focus;
extern Panel g_active;

extern char g_sys_msg[MAX_SYS_MSG];
extern time_t g_sys_msg_time;
extern SysMsgType g_sys_msg_type;

extern pthread_mutex_t g_ui_mutex;

extern char g_token[MAX_TOKEN_LEN];
extern char g_login[MAX_LOGIN_LEN];
