#include <history.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

#include <vars.h>
#include <utils.h>
#include <handler.h>

static void fmt_msg(const char *name, const char *text, char *out, size_t max)
{
    time_t t = time(NULL);
    char ts[8];
    strftime(ts, sizeof(ts), "%H:%M", localtime(&t));
    snprintf(out, max, "[%s] %s: %s", ts, name, text);
}

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
static void get_client_ip(int fd, char *buf, size_t size)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getpeername(fd, (struct sockaddr *)&addr, &len) == 0)
        inet_ntop(AF_INET, &addr.sin_addr, buf, size);
    else
        strncpy(buf, "unknown", size - 1);
}
void handle_client(ClientCtx ctx)
{
    int fd = ctx.fd;
    Shared *sh = ctx.sh;
    char name[NAME_LEN];
    char client_ip[INET_ADDRSTRLEN];
    get_client_ip(fd, client_ip, sizeof(client_ip));

    if (net_readline(fd, name, sizeof(name)) <= 0)
    {
        close(fd);
        exit(0);
    }
    strip_nl(name);

    char resp[NAME_LEN + 16];
    snprintf(resp, sizeof(resp), "SERVER:%s\n", ctx.srv_name);
    send(fd, resp, strlen(resp), MSG_NOSIGNAL);

    // если подключился сканер то закрываем соединение
    if (strcmp(name, "PROBE") == 0)
    {
        close(fd);
        exit(0);
    }
    printf("[+] %s (%s) подключился\n", name, client_ip);
    fflush(stdout);
    hist_send_to(fd);

    int last = sh->total;
    char msg[FMT_MSG_LEN];

    fmt_event(name, "вошёл в чат", msg, sizeof(msg));
    publish(sh, msg);

    while (1)
    {
        fd_set r;
        FD_ZERO(&r);
        FD_SET(fd, &r);
        struct timeval tv = {0, 100000};
        int ret = select(fd + 1, &r, NULL, NULL, &tv);
        if (ret < 0)
        {
            break;
        }
        // рассылаем новые сообщения (broadcast)
        sem_wait(&sh->mutex);
        int cur = sh->total;
        sem_post(&sh->mutex);

        while (last < cur)
        {
            char m[MSG_LEN];
            sem_wait(&sh->mutex);
            // копируем из shared сюда, чтобы не держать семафор слишком долго.
            // иначе у нас бы блокировались остальные клиенты во время send
            strncpy(m, sh->msgs[last % MAX_MSGS], MSG_LEN - 1);
            sem_post(&sh->mutex);
            send(fd, m, strlen(m), MSG_NOSIGNAL);
            send(fd, "\n", 1, MSG_NOSIGNAL);
            last++;
        }

        // читаем сообщение клиента
        if (ret > 0 && FD_ISSET(fd, &r))
        {
            char buf[MSG_LEN];
            int n = net_readline(fd, buf, sizeof(buf));
            if (n <= 0)
                break;
            strip_nl(buf);
            if (!*buf || (n <= 0) || strcmp(buf, "/quit") == 0)
                break;

            fmt_msg(name, buf, msg, sizeof(msg));
            publish(sh, msg);
        }
    }
    printf("[-] %s (%s) отключился\n", name, client_ip);
    fflush(stdout);
    fmt_event(name, "покинул чат", msg, sizeof(msg));
    publish(sh, msg);
    close(fd);
    exit(0);
}
