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
#include <utils.h>

#define SUBNET_HOST_COUNT (1 << 8) - 2

static int is_local_iface(struct ifaddrs *iface)
{
    if (!iface->ifa_addr || iface->ifa_addr->sa_family != AF_INET)
        return 0;
    if (strcmp(iface->ifa_name, "lo") == 0)
        return 0;
    return 1;
}

// получает первую локальную подсеть
static int get_subnet(char *subnet)
{
    struct ifaddrs *list;
    if (getifaddrs(&list) != 0)
        return;

    for (struct ifaddrs *iface = list; iface; iface = iface->ifa_next)
    {
        if (!is_local_iface(iface))
            continue;

        get_subnet_from_iface(iface, subnet);
        break;
    }

    freeifaddrs(list);
    return 0;
}

// сканирует подсеть на наличие серверов
static int scan_subnet(char *subnet, ServerInfo *servers, int max_count)
{
    int fds[SUBNET_HOST_COUNT];
    char ips[SUBNET_HOST_COUNT][20];
    struct pollfd pfds[SUBNET_HOST_COUNT];

    // Неблокирующие connect ко всем хостам подсети
    for (int i = 0; i < SUBNET_HOST_COUNT; i++)
    {
        snprintf(ips[i], sizeof(ips[i]), "%s.%d", subnet, i + 1);

        fds[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (fds[i] < 0)
        {
            pfds[i].fd = -1;
            continue;
        }
        set_socket_blocking(fds[i], 0);

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT);
        if (inet_pton(AF_INET, ips[i], &addr.sin_addr) != 1)
        {
            close(fds[i]);
            fds[i] = -1;
            pfds[i].fd = -1;
            continue;
        }

        connect(fds[i], (struct sockaddr *)&addr, sizeof(addr));
        pfds[i].fd = fds[i];
        pfds[i].events = POLLOUT; // Чего ждем: готовности к записи (соединение установлено)
        pfds[i].revents = 0;      // сюда результат
    }

    poll(pfds, SUBNET_HOST_COUNT, 500); // ожидание

    // Проверка каждого
    int count = 0;
    for (int i = 0; i < SUBNET_HOST_COUNT; i++)
    {
        if (fds[i] < 1)
        {
            close(fds[i]);
            continue;
        }
        if (!(pfds[i].revents & POLLOUT))
            continue;

        int err = 0;
        socklen_t elen = sizeof(err);
        getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &err, &elen);
        if (err)
        {
            close(fds[i]);
            continue;
        }

        set_socket_blocking(fds[i], 1);
        send(fds[i], "PROBE", 5, 0);

        struct pollfd wait = {fds[i], POLLIN, 0};
        if (poll(&wait, 1, 1000) <= 0 || !(wait.revents & POLLIN))
        {
            close(fds[i]);
            continue;
        }

        char buf[256] = {0};
        int n = (int)recv(fds[i], buf, sizeof(buf) - 1, 0);
        close(fds[i]);
        if (n <= 0)
            continue;
        buf[n] = '\0';

        char *p = strstr(buf, "SERVER:");
        if (!p)
            continue;

        p += 7;
        char *end = strchr(p, '\n');
        if (end)
            *end = '\0';

        strncpy(servers[count].ip, ips[i], sizeof(servers[count].ip) - 1);
        strncpy(servers[count].name, p, NAME_LEN - 1);
        count++;
    }

    return count;
}

int get_servers(ServerInfo *servers)
{
    char *subnet[INET_ADDRSTRLEN];
    get_subnet(subnet);
    return scan_subnet(subnet, servers, 10);
}