#include "handler.h"
#include "history.h"
#include "../common/net_utils.h"
#include "../common/protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/* Форматирует сообщение пользователя */
static void fmt_msg(const char *name, const char *text, char *out, size_t max)
{
    time_t t = time(NULL);
    char ts[8];
    strftime(ts, sizeof(ts), "%H:%M", localtime(&t));
    snprintf(out, max, "[%s] %s: %s", ts, name, text);
}

/* Форматирует системное событие */
static void fmt_event(const char *name, const char *ev, char *out, size_t max)
{
    time_t t = time(NULL);
    char ts[8];
    strftime(ts, sizeof(ts), "%H:%M", localtime(&t));
    snprintf(out, max, "[%s] *** %s %s ***", ts, name, ev);
}

static void publish(Shared *sh, const char *msg)
{
    hist_append(msg);
    shared_broadcast(sh, msg);
}

void handle_client(ClientCtx ctx)
{
    int fd = ctx.fd;
    Shared *sh = ctx.sh;
    char name[NAME_LEN];

    if (net_readline(fd, name, sizeof(name)) <= 0) { close(fd); exit(0); }
    net_strip_nl(name);

    char resp[NAME_LEN + 16];
    snprintf(resp, sizeof(resp), "SERVER:%s\n", ctx.srv_name);
    send(fd, resp, strlen(resp), MSG_NOSIGNAL);

    /* Зондирование */
    if (strcmp(name, "PROBE") == 0) { close(fd); exit(0); }

    /* История */
    hist_send_to(fd);
    send(fd, "END_HISTORY\n", 12, MSG_NOSIGNAL);

    int last = sh->total;

    char msg[MSG_LEN];
    fmt_event(name, "вошёл в чат", msg, sizeof(msg));
    publish(sh, msg);

    while (1) {
        fd_set r; FD_ZERO(&r); FD_SET(fd, &r);
        struct timeval tv = {0, 100000};
        int ret = select(fd + 1, &r, NULL, NULL, &tv);
        if (ret < 0) { if (errno == EINTR) continue; break; }

        /* Рассылаем новые сообщения */
        sem_wait(&sh->mutex);
        int cur = sh->total;
        sem_post(&sh->mutex);

        while (last < cur) {
            char m[MSG_LEN];
            sem_wait(&sh->mutex);
            strncpy(m, sh->msgs[last % MAX_MSGS], MSG_LEN - 1);
            sem_post(&sh->mutex);
            send(fd, m, strlen(m), MSG_NOSIGNAL);
            send(fd, "\n", 1, MSG_NOSIGNAL);
            last++;
        }

        /* Читаем сообщение клиента */
        if (ret > 0 && FD_ISSET(fd, &r)) {
            char buf[MSG_LEN];
            int n = net_readline(fd, buf, sizeof(buf));
            if (n <= 0) break;
            net_strip_nl(buf);
            if (!*buf || strcmp(buf, "/quit") == 0) break;

            fmt_msg(name, buf, msg, sizeof(msg));
            publish(sh, msg);
        }
    }

    fmt_event(name, "покинул чат", msg, sizeof(msg));
    publish(sh, msg);
    close(fd);
    exit(0);
}
