#include "history.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>

static char hist_path[256];

void hist_init(const char *path)
{
    strncpy(hist_path, path, sizeof(hist_path) - 1);
}

void hist_append(const char *msg)
{
    FILE *f = fopen(hist_path, "a");
    if (!f) return;
    fputs(msg, f);
    fputc('\n', f);
    fclose(f);
}

void hist_send_to(int fd)
{
    FILE *f = fopen(hist_path, "r");
    if (!f) return;
    char line[MSG_LEN];
    while (fgets(line, sizeof(line), f))
        send(fd, line, strlen(line), MSG_NOSIGNAL);
    fclose(f);
}
