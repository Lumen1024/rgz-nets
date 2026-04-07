#pragma once

// Delete last UTF-8 character from buf (len = current strlen). Returns new length.
int utf8_backspace(char *buf, int len);

// Main ncurses event loop. Returns when user presses 'q'.
// Declared in ui.h — defined in ui/input.c
