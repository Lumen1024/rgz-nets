#pragma once

/* Читает строку из сокета побайтово до '\n'. Возвращает кол-во байт или <=0. */
int  net_readline(int fd, char *buf, int max);

/* Обрезает \r\n в конце строки. */
void net_strip_nl(char *s);