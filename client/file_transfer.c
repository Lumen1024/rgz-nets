#include <file_transfer.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/stat.h>

#define FT_BUF_SIZE 4096

void *ft_send(void *args) {
    FtSendArgs *a = (FtSendArgs *)args;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons((uint16_t)a->port);
    inet_pton(AF_INET, a->ip, &addr.sin_addr);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return NULL;
    }

    int file = open(a->filepath, O_RDONLY);
    if (file < 0) {
        close(fd);
        return NULL;
    }

    char buf[FT_BUF_SIZE];
    ssize_t n;
    while ((n = read(file, buf, sizeof(buf))) > 0) {
        ssize_t sent = 0;
        while (sent < n) {
            ssize_t w = write(fd, buf + sent, (size_t)(n - sent));
            if (w <= 0) goto done;
            sent += w;
        }
    }

done:
    close(file);
    close(fd);
    return NULL;
}

void *ft_receive(void *args) {
    FtReceiveArgs *a = (FtReceiveArgs *)args;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) return NULL;

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons((uint16_t)a->port);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(server_fd);
        return NULL;
    }

    if (listen(server_fd, 1) < 0) {
        close(server_fd);
        return NULL;
    }

    int client_fd = accept(server_fd, NULL, NULL);
    close(server_fd);
    if (client_fd < 0) return NULL;

    int file = open(a->save_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file < 0) {
        close(client_fd);
        return NULL;
    }

    char buf[FT_BUF_SIZE];
    ssize_t n;
    while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
        write(file, buf, (size_t)n);
    }

    close(file);
    close(client_fd);
    return NULL;
}
