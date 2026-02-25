#pragma once

#include <ifaddrs.h>

int net_readline(int fd, char *buf, int max);

void strip_nl(char *s);

void set_socket_blocking(int fd, int value);

void get_subnet_from_iface(struct ifaddrs *iface, char *subnet);

// Создаёт TCP-соединение. Возвращает fd
int create_tcp_connection(const char *ip, const short port);