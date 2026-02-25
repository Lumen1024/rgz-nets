#include <utils.h>

#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ifaddrs.h>

int net_readline(int fd, char *buf, int max)
{
    int n = 0;
    char c;
    while (n < max - 1 && recv(fd, &c, 1, 0) == 1)
    {
        buf[n++] = c;
        if (c == '\n')
            break;
    }
    buf[n] = '\0';
    return n;
}

void strip_nl(char *s)
{
  s[strcspn(s, "\r\n")] = '\0';
}

void set_socket_blocking(int fd, int value)
{
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, value ? (flags & ~O_NONBLOCK) : (flags | O_NONBLOCK));
}

void get_subnet_from_iface(struct ifaddrs *iface, char *subnet)
{
  char tmp[INET_ADDRSTRLEN];
  struct sockaddr_in *ipv4 = (struct sockaddr_in *)iface->ifa_addr;
  inet_ntop(AF_INET, &ipv4->sin_addr, tmp, sizeof(tmp));

  char *last_dot = strrchr(tmp, '.');
  size_t len = (size_t)(last_dot - tmp);
  memcpy(subnet, tmp, len);
  subnet[len] = '\0';
}

int create_tcp_connection(const char *ip, const short port)
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
    return -1;

  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ip, &addr.sin_addr) != 1)
  {
    close(fd);
    return -1;
  }

  if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
  {
    close(fd);
    return -1;
  }

  return fd;
}