#pragma once

void hist_init(const char *path);

void hist_append(const char *msg);

void hist_send_to(int fd); /* отправляет всю историю в сокет */
