#pragma once

#include <protocol.h>

// Right panel display modes
typedef enum { LIST_MODE_CHATS = 0, LIST_MODE_USERS = 1, LIST_MODE_MEMBERS = 2 } ListMode;

void ui_init();
void ui_destroy();
int ui_prompt_server(char *host_out, int *port_out);
int ui_prompt_login(char *login_out, char *password_out);
int ui_prompt_register(char *login_out, char *password_out);
void ui_run();
void ui_set_chat(const char *chat_name, Message *msgs, int count);
void ui_set_chat_list(char **names, int count);
// names: all server users; has_messages[i]: 1 if dialog exists, 0 if not
void ui_set_user_list(char **names, int *has_messages, int count);
void ui_set_member_list(char **names, int count);
void ui_append_message(Message *msg);
void ui_notify(const char *text);
void ui_sys(const char *text); // write to system bar
const char *ui_get_current_chat(void); // returns current open chat route
