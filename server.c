#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define PORT      8888
#define MAX_MSGS  1000
#define MSG_LEN   512
#define NAME_LEN  64
#define BACKLOG   16

/* Разделяемая память между дочерними процессами (fork) */
typedef struct {
    sem_t mutex;
    int   total;                     /* сколько сообщений записано всего */
    char  msgs[MAX_MSGS][MSG_LEN];   /* кольцевой буфер сообщений        */
} Shared;

static Shared *sh;
static char   srv_name[NAME_LEN] = "Чат";
static char   hist_file[256];

/* ── Утилиты ──────────────────────────────────────────────────────── */

/* Читает одну строку (до '\n') из сокета побайтово. */
static int fd_readline(int fd, char *buf, int max)
{
    int n = 0;
    char c;
    while (n < max - 1) {
        int r = (int)recv(fd, &c, 1, 0);
        if (r <= 0) return r;
        buf[n++] = c;
        if (c == '\n') break;
    }
    buf[n] = '\0';
    return n;
}

static void strip_nl(char *s) { s[strcspn(s, "\r\n")] = '\0'; }

/* Дописывает строку в файл истории. */
static void hist_append(const char *msg)
{
    FILE *f = fopen(hist_file, "a");
    if (!f) return;
    fputs(msg, f);
    fputc('\n', f);
    fclose(f);
}

/* Добавляет сообщение в кольцевой буфер разделяемой памяти. */
static void broadcast(const char *msg)
{
    sem_wait(&sh->mutex);
    int idx = sh->total % MAX_MSGS;
    strncpy(sh->msgs[idx], msg, MSG_LEN - 1);
    sh->msgs[idx][MSG_LEN - 1] = '\0';
    sh->total++;
    sem_post(&sh->mutex);
}

/* Форматирует сообщение пользователя: "[ЧЧ:ММ] Имя: текст" */
static void fmt_msg(const char *name, const char *text, char *out, size_t max)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[8];
    strftime(ts, sizeof(ts), "%H:%M", tm);
    snprintf(out, max, "[%s] %s: %s", ts, name, text);
}

/* Форматирует системное событие: "[ЧЧ:ММ] *** Имя событие ***" */
static void fmt_event(const char *name, const char *ev, char *out, size_t max)
{
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[8];
    strftime(ts, sizeof(ts), "%H:%M", tm);
    snprintf(out, max, "[%s] *** %s %s ***", ts, name, ev);
}

/* ── Обработчик клиента (дочерний процесс) ──────────────────────── */

static void handle_client(int fd)
{
    char name[NAME_LEN];

    /* Получаем имя пользователя (или "PROBE" для сканирования сети) */
    if (fd_readline(fd, name, sizeof(name)) <= 0) { close(fd); exit(0); }
    strip_nl(name);

    /* Зондирование: просто отвечаем именем сервера и закрываем */
    if (strcmp(name, "PROBE") == 0) {
        char resp[NAME_LEN + 16];
        snprintf(resp, sizeof(resp), "SERVER:%s\n", srv_name);
        send(fd, resp, strlen(resp), 0);
        close(fd);
        exit(0);
    }

    /* Отправляем имя сервера */
    {
        char resp[NAME_LEN + 16];
        snprintf(resp, sizeof(resp), "SERVER:%s\n", srv_name);
        send(fd, resp, strlen(resp), 0);
    }

    /* Отправляем историю чата из файла */
    {
        FILE *f = fopen(hist_file, "r");
        if (f) {
            char line[MSG_LEN];
            while (fgets(line, sizeof(line), f))
                send(fd, line, strlen(line), 0);
            fclose(f);
        }
        send(fd, "END_HISTORY\n", 12, 0);
    }

    /*
     * Запоминаем текущее количество сообщений в буфере,
     * чтобы не повторять историю через broadcast-канал.
     */
    int last = sh->total;

    /* Объявляем о приходе пользователя */
    {
        char msg[MSG_LEN];
        fmt_event(name, "вошёл в чат", msg, sizeof(msg));
        hist_append(msg);
        broadcast(msg);
    }

    /* ── Основной цикл: принимаем сообщения и рассылаем новые ── */
    while (1) {
        fd_set r;
        FD_ZERO(&r);
        FD_SET(fd, &r);
        struct timeval tv = {0, 100000}; /* 100 мс */
        int ret = select(fd + 1, &r, NULL, NULL, &tv);

        if (ret < 0) { if (errno == EINTR) continue; break; }

        /* Пересылаем новые сообщения из разделяемого буфера клиенту */
        int cur;
        sem_wait(&sh->mutex);
        cur = sh->total;
        sem_post(&sh->mutex);

        while (last < cur) {
            char m[MSG_LEN];
            sem_wait(&sh->mutex);
            strncpy(m, sh->msgs[last % MAX_MSGS], MSG_LEN - 1);
            sem_post(&sh->mutex);
            send(fd, m, strlen(m), 0);
            send(fd, "\n", 1, 0);
            last++;
        }

        /* Читаем сообщение от клиента */
        if (ret > 0 && FD_ISSET(fd, &r)) {
            char buf[MSG_LEN];
            int n = fd_readline(fd, buf, sizeof(buf));
            if (n <= 0) break;
            strip_nl(buf);
            if (!*buf) continue;
            if (strcmp(buf, "/quit") == 0) break;

            char msg[MSG_LEN];
            fmt_msg(name, buf, msg, sizeof(msg));
            hist_append(msg);
            broadcast(msg);
        }
    }

    /* Объявляем об уходе пользователя */
    {
        char msg[MSG_LEN];
        fmt_event(name, "покинул чат", msg, sizeof(msg));
        hist_append(msg);
        broadcast(msg);
    }

    close(fd);
    exit(0);
}

/* ── SIGCHLD: зомби-процессов не будет ─────────────────────────── */

static void sigchld_handler(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* ── main ───────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    if (argc >= 2) strncpy(srv_name, argv[1], NAME_LEN - 1);
    snprintf(hist_file, sizeof(hist_file), "chat_%s.txt", srv_name);

    /* Разделяемая память для межпроцессного broadcast */
    sh = mmap(NULL, sizeof(Shared),
              PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sh == MAP_FAILED) { perror("mmap"); return 1; }
    sem_init(&sh->mutex, /*pshared=*/1, 1);
    sh->total = 0;

    /* Автоматический сбор завершённых дочерних процессов */
    struct sigaction sa = {0};
    sa.sa_handler = sigchld_handler;
    sa.sa_flags   = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    /* TCP-сокет */
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(PORT);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(srv, BACKLOG) < 0) { perror("listen"); return 1; }

    printf("Сервер «%s» запущен на порту %d\n", srv_name, PORT);
    printf("История чата: %s\n", hist_file);

    /* Показываем локальные IP-адреса */
    struct ifaddrs *ifa_list, *ifa;
    if (getifaddrs(&ifa_list) == 0) {
        for (ifa = ifa_list; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) continue;
            if (strcmp(ifa->ifa_name, "lo") == 0) continue;
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET,
                      &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
                      ip, sizeof(ip));
            printf("Адрес: %s (%s)\n", ip, ifa->ifa_name);
        }
        freeifaddrs(ifa_list);
    }
    printf("Ожидание подключений...\n\n");

    /* Основной цикл: принимаем соединения */
    while (1) {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli = accept(srv, (struct sockaddr *)&cli_addr, &cli_len);
        if (cli < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

        printf("Подключение: %s\n", inet_ntoa(cli_addr.sin_addr));

        pid_t pid = fork();
        if (pid < 0)       { perror("fork"); close(cli); }
        else if (pid == 0) { close(srv); handle_client(cli); /* exit внутри */ }
        else               { close(cli); }
    }
}
