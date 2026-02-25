#define _GNU_SOURCE
#include <session.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <vars.h>
#include <utils.h>

static volatile int is_active;
static int chat_fd;

/* Фоновый поток: читает входящие сообщения и печатает их */
static void *recv_thread(void *unused)
{
    (void)unused;
    char line[MSG_LEN];
    while (is_active)
    {
        int n = net_readline(chat_fd, line, sizeof(line));
        if (n <= 0)
        {
            if (is_active)
                printf("\n[Соединение с сервером разорвано]\n");
            is_active = 0;
            break;
        }
        printf("%s", line);
        fflush(stdout);
    }
    return NULL; /* ← исправлен баг: было return; */
}

/* Читает историю, запускает фоновый поток, обрабатывает ввод */
static void chat_loop(int fd, const char *srv_name)
{
    chat_fd = fd;
    is_active = 1;

    /* Читаем историю до маркера END_HISTORY */
    printf("\n=== История чата «%s» ===\n", srv_name);
    char line[MSG_LEN];
    while (1)
    {
        int n = net_readline(fd, line, sizeof(line));
        if (n <= 0)
            return;
        if (strncmp(line, "END_HISTORY", 11) == 0)
            break;
        printf("%s", line);
    }
    printf("=== Начало чата ===\n");
    printf("(введите /quit для выхода)\n\n");

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, NULL);

    /* Цикл отправки сообщений */
    char input[MSG_LEN];
    while (is_active)
    {
        if (!fgets(input, sizeof(input), stdin))
            break;
        strip_nl(input);

        if (!is_active || !*input)
            continue;

        char msg[MSG_LEN + 2];
        snprintf(msg, sizeof(msg), "%s\n", input);
        if (send(fd, msg, strlen(msg), MSG_NOSIGNAL) < 0)
            break;

        if (strcmp(input, "/quit") == 0)
        {
            is_active = 0;
            break;
        }
    }

    is_active = 0;
    shutdown(fd, SHUT_RDWR); /* прерывает recv в фоновом потоке */
    pthread_join(tid, NULL);
}

/* Создаёт TCP-соединение. Возвращает fd или -1. */
static int make_connection(const char *ip)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
    {
        printf("Неверный IP-адрес: %s\n", ip);
        close(fd);
        return -1;
    }
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        close(fd);
        return -1;
    }
    return fd;
}

void session_run(const char *ip, const char *fallback_name, const char *username)
{
    int fd = make_connection(ip);
    if (fd < 0)
        return;

    /* Отправляем имя пользователя */
    char login[NAME_LEN + 2];
    snprintf(login, sizeof(login), "%s\n", username);
    send(fd, login, strlen(login), MSG_NOSIGNAL);

    /* Читаем имя сервера из ответа "SERVER:имя\n" */
    char srv_name[NAME_LEN];
    strncpy(srv_name, fallback_name, NAME_LEN - 1);

    char resp[MSG_LEN];
    if (net_readline(fd, resp, sizeof(resp)) > 0)
    {
        char *p = strstr(resp, "SERVER:");
        if (p)
        {
            p += 7;
            strip_nl(p);
            strncpy(srv_name, p, NAME_LEN - 1);
        }
    }

    chat_loop(fd, srv_name);
    close(fd);
    printf("\nОтключён от сервера.\n");
}
