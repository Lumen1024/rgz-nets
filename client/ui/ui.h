#pragma once

#include <protocol.h>
#include <api/model.h>

// lifecycle
void ui_init();
void ui_destroy();
void ui_redraw();

// pre-ncurses prompts
int ui_prompt_server(char *host_out, int *port_out);
int ui_prompt_login(char *login_out, char *password_out);
int ui_prompt_register(char *login_out, char *password_out);
void read_line(const char *prompt, char *out, int maxlen, int hidden);

// data setters
void ui_set_chat(const char *chat_name, Message *msgs, int count);
void ui_set_chat_list(char **names, int count);
void ui_set_user_list(char **names, int *has_messages, int count);
void ui_set_member_list(char **names, int count);
void ui_append_message(Message *msg);
void ui_clear_chat();

// notifications and status
void ui_notify(const char *text);
void ui_sys(const char *text);
int ui_has_notify();
void ui_clear_notify();
const char *ui_get_current_chat();

// panel focus/active
Panel ui_get_focus();
Panel ui_get_active();
void ui_set_focus(Panel p);
void ui_set_active(Panel p);

// list navigation
ListMode ui_get_list_mode();
void ui_set_list_mode(ListMode mode);
int ui_get_list_selected();
void ui_set_list_selected(int idx);
int ui_get_list_count();
const char *ui_get_list_item(int idx);

// chat scroll
void ui_scroll_up();
void ui_scroll_down();

// chat input
const char *ui_get_input();
int ui_get_input_len();
void ui_input_append(char ch);
void ui_input_backspace();
void ui_input_clear();

// sys bar input
const char *ui_get_sys_input();
int ui_get_sys_input_len();
void ui_sys_input_append(char ch);
void ui_sys_input_backspace();
void ui_sys_input_clear();
