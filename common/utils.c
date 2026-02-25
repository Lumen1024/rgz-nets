#include <string.h>
#include <sys/socket.h>

#include <utils.h>

int net_readline(int fd, char *buf, int max)
{
    int n = 0;
    char c;
    while (n < max - 1 && recv(fd, &c, 1, 0) == 1)
    {
        buf[n++] = c;
        if (c == '\n')
            break;
    }
    buf[n] = '\0';
    return n;
}

void strip_nl(char *s)
{
    s[strcspn(s, "\r\n")] = '\0';
}