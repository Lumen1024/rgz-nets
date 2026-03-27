#pragma once

// Returns 1 if g_current_chat is a group chat and writes its name to out.
int current_chat_name(char *out, int maxlen);

// Returns 1 if the current chat is a private dialog (/users/...).
int is_private_chat(void);

// Process a slash-command string (e.g. "/create foo").
void handle_command(const char *cmd);

// Called when the user presses Enter in the system bar.
void handle_sys_input(void);
