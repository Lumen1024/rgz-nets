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
#include <notify.h>

#define DEFAULT_PORT 8080
#define BACKLOG 16

static void reap_children(int sig)
{
    (void)sig;
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
    {
        notify_parent_unregister(pid);
    }
}

int main(int argc, char *argv[])
{
    int port = DEFAULT_PORT;
    if (argc == 2)
    {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535)
        {
            fprintf(stderr, "invalid port: %s\n", argv[1]);
            return 1;
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, BACKLOG) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    notify_init();

    printf("server listening on port %d\n", port);

    struct sigaction sa;
    sa.sa_handler = reap_children;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    while (1)
    {
        // Dispatch any pending notifications from children
        notify_dispatch();

        // Non-blocking accept via select with short timeout
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(server_fd, &rfds);
        struct timeval tv = {0, 10000}; // 10ms
        int ready = select(server_fd + 1, &rfds, NULL, NULL, &tv);
        if (ready <= 0)
            continue;

        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("client connected: %s\n", inet_ntoa(client_addr.sin_addr));
        fflush(stdout);

        // Create pipe: parent reads [0], child writes [1]
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

        if (pid == 0)
        {
            // Child: close server fd and pipe read-end
            close(server_fd);
            close(pipefd[0]);
            notify_child_init(pipefd[1]);
            handle_client(client_fd);
            close(pipefd[1]);
            exit(0);
        }

        // Parent: keep a dup of client_fd for sending notifications,
        // then close the original (child has its own copy from fork).
        int parent_client_fd = dup(client_fd);
        close(pipefd[1]);
        close(client_fd);
        notify_parent_register(pid, pipefd[0], parent_client_fd);
    }

    close(server_fd);
    return 0;
}
