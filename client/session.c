#define _GNU_SOURCE
#include <session.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdatomic.h>

#include <vars.h>
#include <utils.h>

static atomic_int is_active;
static int chat_fd;

// Фоновый поток: читает входящие сообщения и печатает их
static void *rececive_messages_thread(void *unused)
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
    return NULL;
}

static void chat_loop(int fd)
{
    chat_fd = fd;
    is_active = 1;

    // История до END_HISTORY
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

    pthread_t tid;
    pthread_create(&tid, NULL, rececive_messages_thread, NULL);

    // Цикл отправки сообщений
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

    shutdown(fd, SHUT_RDWR); // чтобы recv не ждал
    pthread_join(tid, NULL);
}

int run_session(const char *ip, const char *username)
{
    int fd = create_tcp_connection(ip, PORT);
    if (fd < 0)
        return -1;

    // Отправляем имя пользователя
    char login[NAME_LEN + 2];
    snprintf(login, sizeof(login), "%s\n", username);
    send(fd, login, strlen(login), MSG_NOSIGNAL);

    char server_name[NAME_LEN];
    char resp[MSG_LEN];
    if (net_readline(fd, resp, sizeof(resp)) > 0)
    {
        char *p = strstr(resp, "SERVER:");
        if (!p)
            return -1;
        p += 7;
        strip_nl(p);
        strncpy(server_name, p, NAME_LEN - 1);
    }

    chat_loop(fd);
    close(fd);
    printf("\nОтключён от сервера.\n");
    return 0;
}
