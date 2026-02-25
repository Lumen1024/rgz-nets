#define _GNU_SOURCE
#include <scanner.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <vars.h>

#define SUBNET_HOST_COUNT 254

static void set_nonblocking(int fd, int on)
{
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK));
}

static void get_subnet(char *out, size_t size)
{
    strncpy(out, "192.168.1", size - 1);

    struct ifaddrs *list;
    if (getifaddrs(&list) != 0)
        return;

    for (struct ifaddrs *iface = list; iface; iface = iface->ifa_next)
    {
        if (!iface->ifa_addr || iface->ifa_addr->sa_family != AF_INET)
            continue;
        if (strcmp(iface->ifa_name, "lo") == 0)
            continue;

        char ip_str[INET_ADDRSTRLEN];
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)iface->ifa_addr;
        inet_ntop(AF_INET, &ipv4->sin_addr, ip_str, sizeof(ip_str));

        char *last_dot = strrchr(ip_str, '.');
        size_t len = (size_t)(last_dot - ip_str);
        if (len >= size)
            len = size - 1;
        strncpy(out, ip_str, len);
        out[len] = '\0';
        break;
    }

    freeifaddrs(list);
}

int scan_local_network(ServerInfo *out, int max_count)
{
    char subnet[16];
    get_subnet(subnet, sizeof(subnet));
    printf("Сканирование %s.* (порт %d)...\n", subnet, PORT); /* ← исправлен баг: был лишний subnet */

    int socks[SUBNET_HOST_COUNT];
    char ips[SUBNET_HOST_COUNT][20];
    struct pollfd pfds[SUBNET_HOST_COUNT];

    /* Запускаем неблокирующие connect ко всем хостам подсети */
    for (int i = 0; i < SUBNET_HOST_COUNT; i++)
    {
        snprintf(ips[i], sizeof(ips[i]), "%s.%d", subnet, i + 1);

        socks[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (socks[i] < 0)
        {
            pfds[i].fd = -1;
            continue;
        }
        set_nonblocking(socks[i], 1);

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, ips[i], &addr.sin_addr) != 1)
        {
            close(socks[i]);
            socks[i] = -1;
            pfds[i].fd = -1;
            continue;
        }

        connect(socks[i], (struct sockaddr *)&addr, sizeof(addr)); /* EINPROGRESS — ок */
        pfds[i].fd = socks[i];
        pfds[i].events = POLLOUT;
        pfds[i].revents = 0;
    }

    poll(pfds, SUBNET_HOST_COUNT, 500); /* ждём 500 мс */

    int count = 0;
    for (int i = 0; i < SUBNET_HOST_COUNT && count < max_count; i++)
    {
        if (socks[i] < 0 || !(pfds[i].revents & POLLOUT))
        {
            if (socks[i] >= 0)
                close(socks[i]);
            continue;
        }

        /* Проверяем, не было ли ошибки соединения */
        int err = 0;
        socklen_t elen = sizeof(err);
        getsockopt(socks[i], SOL_SOCKET, SO_ERROR, &err, &elen);
        if (err)
        {
            close(socks[i]);
            continue;
        }

        set_nonblocking(socks[i], 0);

        /* Отправляем зонд и ждём ответа */
        send(socks[i], "PROBE\n", 6, 0); /* ← исправлен баг: теперь "PROBE\n" */

        struct pollfd wait = {socks[i], POLLIN, 0};
        if (poll(&wait, 1, 1000) <= 0 || !(wait.revents & POLLIN))
        {
            close(socks[i]);
            continue;
        }

        char buf[256] = {0};
        int n = (int)recv(socks[i], buf, sizeof(buf) - 1, 0);
        close(socks[i]);
        if (n <= 0)
            continue;
        buf[n] = '\0';

        /* Разбираем ответ "SERVER:имя\n" */
        char *p = strstr(buf, "SERVER:");
        if (!p)
            continue;
        p += 7;
        char *end = strchr(p, '\n');
        if (end)
            *end = '\0';

        strncpy(out[count].ip, ips[i], sizeof(out[count].ip) - 1);
        strncpy(out[count].name, p, NAME_LEN - 1);
        printf("  [%d] %s  (%s)\n", count + 1, out[count].name, out[count].ip);
        count++;
    }

    return count;
}
