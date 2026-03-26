#pragma once

#include <protocol.h>

void ui_init();
void ui_destroy();
int ui_prompt_server(char *host_out, int *port_out);
int ui_prompt_login(char *login_out, char *password_out);
int ui_prompt_register(char *login_out, char *password_out);
void ui_run();
void ui_set_chat(const char *chat_name, Message *msgs, int count);
void ui_set_chat_list(char **names, int count);
void ui_append_message(Message *msg);
void ui_notify(const char *text);
