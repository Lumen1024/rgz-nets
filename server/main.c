#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vars.h>
#include <history.h>
#include <shared.h>
#include <handler.h>

static void sigchld_handler(int sig)
{
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

int main(int argc, char *argv[])
{
    char srv_name[NAME_LEN] = "Чат";
    if (argc >= 2)
        strncpy(srv_name, argv[1], NAME_LEN - 1);
    char hist_path[256];
    snprintf(hist_path, sizeof(hist_path), "chat_%s.txt", srv_name);
    hist_init(hist_path);

    Shared *sh = shared_init();

    struct sigaction sa = {0};
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0)
    {
        perror("socket");
        return 1;
    }
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        return 1;
    }
    if (listen(srv, BACKLOG) < 0)
    {
        perror("listen");
        return 1;
    }

    printf("Сервер «%s» запущен на порту %d\n", srv_name, PORT);

    while (1)
    {
        struct sockaddr_in cli_addr;
        socklen_t cli_len = sizeof(cli_addr);
        int cli = accept(srv, (struct sockaddr *)&cli_addr, &cli_len);
        if (cli < 0)
        {
            perror("accept");
            continue;
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            close(cli);
        }
        else if (pid == 0)
        {
            close(srv);
            ClientCtx ctx;
            ctx.fd = cli;
            ctx.sh = sh;
            snprintf(ctx.srv_name, NAME_LEN, "%s", srv_name);
            handle_client(ctx);
        }
        else
        {
            close(cli);
        }
    }
}
