#include <socket_utils.h>

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int read_message(int socket_fd, char *buffer, size_t max_len) {
    size_t total = 0;

    while (total < max_len - 1) {
        ssize_t n = read(socket_fd, buffer + total, 1);
        if (n <= 0) {
            return -1;
        }
        if (buffer[total] == '\n') {
            buffer[total] = '\0';
            return 0;
        }
        total++;
    }

    // Buffer full without finding newline
    buffer[total] = '\0';
    return -1;
}

int write_message(int socket_fd, const char *data) {
    size_t len = strlen(data);
    size_t sent = 0;

    // Send the data
    while (sent < len) {
        ssize_t n = write(socket_fd, data + sent, len - sent);
        if (n <= 0) {
            return -1;
        }
        sent += (size_t)n;
    }

    // Send the newline delimiter
    char nl = '\n';
    ssize_t n = write(socket_fd, &nl, 1);
    if (n <= 0) {
        return -1;
    }

    return 0;
}

int get_peer_ip(int socket_fd, char *ip_out) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if (getpeername(socket_fd, (struct sockaddr *)&addr, &addr_len) == -1) {
        return -1;
    }

    const char *result = inet_ntop(AF_INET, &addr.sin_addr, ip_out, INET_ADDRSTRLEN);
    if (result == NULL) {
        return -1;
    }

    return 0;
}
