#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <client_handler.h>
#include <notify/shared.h>
#include <notify/parent.h>
#include <notify/notify.h>

#define DEFAULT_PORT 8080
#define BACKLOG 16 // очередь для принятия соединения

static void reap_children(int sig)
{
    (void)sig;
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        notify_parent_unregister(pid);
    }
}

static void setup_sigchld(void)
{
    struct sigaction sa;
    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}

static int get_port(int argc, char *argv[])
{
    if (argc != 2)
        return DEFAULT_PORT;

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535)
    {
        fprintf(stderr, "invalid port: %s\n", argv[1]);
        exit(1);
    }
    return port;
}

static int configure_server_socket(int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1; // adress reuse
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(fd);
        exit(1);
    }

    if (listen(fd, BACKLOG) < 0)
    {
        perror("listen");
        close(fd);
        exit(1);
    }

    return fd;
}

static int wait_connections(int server_fd)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(server_fd, &rfds);

    struct timeval tv = {0, 10000}; // 10ms
    return select(server_fd + 1, &rfds, NULL, NULL, &tv);
}

static int accept_client(int server_fd)
{
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0)
    {
        perror("accept");
        exit(1);
    }

    printf("client connected: %s\n", inet_ntoa(client_addr.sin_addr));
    fflush(stdout);

    return client_fd;
}

int main(int argc, char *argv[])
{
    int port = get_port(argc, argv);
    int server_fd = configure_server_socket(port);

    notify_init();
    setup_sigchld();

    printf("server port: %d\n", port);

    while (1)
    {
        notify_dispatch();

        if (wait_connections(server_fd) <= 0)
            continue;

        int client_fd = accept_client(server_fd);

        // parent reads [0], child writes [1]
        // for notify
        int pipefd[2];
        if (pipe(pipefd) < 0)
        {
            perror("pipe");
            close(client_fd);
            continue;
        }

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            close(pipefd[0]);
            close(pipefd[1]);
            close(client_fd);
            continue;
        }

        if (pid == 0) // child
        {
            close(server_fd);
            close(pipefd[0]);

            notify_child_init(pipefd[1]);
            handle_client(client_fd);

            close(pipefd[1]);
            exit(0);
        }

        // parent
        int parent_client_fd = dup(client_fd);
        close(pipefd[1]);
        close(client_fd);
        notify_parent_register(pid, pipefd[0], parent_client_fd);
    }

    close(server_fd);
    return 0;
}
