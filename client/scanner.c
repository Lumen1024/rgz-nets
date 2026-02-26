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

// состояния сокетов при сканировании
static enum {
    ST_CONNECTING,
    ST_PROBE_SENT,
    ST_DONE
};

// сканирует подсеть на наличие серверов
static int scan_subnet(char *subnet, ServerInfo *servers, int max_count)
{
    int fds[SUBNET_HOST_COUNT];
    char ips[SUBNET_HOST_COUNT][20];
    struct pollfd pfds[SUBNET_HOST_COUNT];
    int state[SUBNET_HOST_COUNT];

    // connect
    for (int i = 0; i < SUBNET_HOST_COUNT; i++)
    {
        snprintf(ips[i], sizeof(ips[i]), "%s.%d", subnet, i + 1);
        state[i] = ST_DONE;

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
        pfds[i].events = POLLOUT;
        pfds[i].revents = 0;
        state[i] = ST_CONNECTING;
    }

    // connect -> PROBE -> SERVER
    int count = 0;
    for (int round = 0; round < 8; round++)
    {
        int active = 0;
        for (int i = 0; i < SUBNET_HOST_COUNT; i++)
            if (state[i] != ST_DONE)
                active++;
        if (active == 0)
            break;

        poll(pfds, SUBNET_HOST_COUNT, 500);

        for (int i = 0; i < SUBNET_HOST_COUNT; i++)
        {
            if (state[i] == ST_DONE || pfds[i].fd < 0)
                continue;
            if (!pfds[i].revents)
                continue;

            if (state[i] == ST_CONNECTING)
            {
                // проверяем результат connect
                int err = 0;
                socklen_t elen = sizeof(err);
                getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &err, &elen);
                if (err || (pfds[i].revents & (POLLERR | POLLHUP)))
                {
                    close(fds[i]);
                    fds[i] = -1;
                    pfds[i].fd = -1;
                    state[i] = ST_DONE;
                    continue;
                }

                send(fds[i], "PROBE\n", 6, MSG_NOSIGNAL);
                pfds[i].events = POLLIN;
                state[i] = ST_PROBE_SENT;
            }
            else if (state[i] == ST_PROBE_SENT)
            {
                // читаем ответ
                char buf[256] = {0};
                int n = (int)recv(fds[i], buf, sizeof(buf) - 1, 0);
                close(fds[i]);
                fds[i] = -1;
                pfds[i].fd = -1;
                state[i] = ST_DONE;

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

                if (count < max_count)
                {
                    memcpy(servers[count].ip, ips[i], sizeof(ips[i]));
                    strncpy(servers[count].name, p, NAME_LEN - 1);
                    count++;
                }
            }
        }
    }

    for (int i = 0; i < SUBNET_HOST_COUNT; i++)
        if (fds[i] >= 0)
            close(fds[i]);

    return count;
}

int get_servers(ServerInfo *servers)
{
    struct ifaddrs *list;
    if (getifaddrs(&list) != 0)
        return 0;

    int total = 0;
    for (struct ifaddrs *iface = list; iface && total < MAX_SERVERS; iface = iface->ifa_next)
    {
        if (!is_local_iface(iface))
            continue;

        char subnet[INET_ADDRSTRLEN];
        get_subnet_from_iface(iface, subnet);
        total += scan_subnet(subnet, servers + total, MAX_SERVERS - total);
    }

    freeifaddrs(list);
    return total;
}