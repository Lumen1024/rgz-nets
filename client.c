#include <stddef.h>
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8888
#define MAX_SRVS 64
#define MSG_LEN 512
#define NAME_LEN 64
#define SCAN_NUM 254 /* сканируем .1 – .254 */

typedef struct
{
  char ip[32];
  char name[NAME_LEN];
} Server;

static Server srvs[MAX_SRVS];
static int srv_cnt;
static char username[NAME_LEN];
static volatile int running; /* флаг активной сессии для потоков */
static int chat_fd;

// Читает одну строку из сокета
static int fd_readline(int fd, char *buf, int max)
{
  for (int n = 0; n < max - 1; n++)
  {
    char c;
    if (recv(fd, &c, 1, 0) < 0)
      return -1;

    buf[n] = c;

    if (c == '\n')
    {
      buf[n + 1] = '\0';
      return n + 1;
    }
  }

  buf[max - 1] = '\0';
  return max - 1;
}

// Обрезает строку до первого \n \r
static void strip_nl(char *s)
{
  size_t end_index = strcspn(s, "\r\n");
  s[end_index] = '\0';
}

// Определяет первую не-loopback подсеть
static void get_subnet(char *out, size_t len)
{

  // односвязный список интерфейсов
  struct ifaddrs *interface_list, *cur;

  strncpy(out, "192.168.1", len - 1);
  if (getifaddrs(&interface_list) != 0)
    return;

  for (cur = interface_list; cur; cur = cur->ifa_next)
  {

    if (!cur->ifa_addr || cur->ifa_addr->sa_family != AF_INET)
      continue;
    if (strcmp(cur->ifa_name, "lo") == 0)
      continue;

    struct sockaddr_in *sin = (struct sockaddr_in *)cur->ifa_addr;
    char ip[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &sin->sin_addr, ip, sizeof(ip));
    char *dot = strrchr(ip, '.');
    if (dot)
    {
      size_t pl = (size_t)(dot - ip);
      if (pl >= len)
        pl = len - 1;
      strncpy(out, ip, pl);
      out[pl] = '\0';
    }
    break;
  }

  freeifaddrs(interface_list);
}

/*
 * Параллельное сканирование /24 подсети:
 *  1. Открываем 254 неблокирующих сокета и вызываем connect() на всех.
 *  2. Ждём poll() 500 мс — устройства в LAN откликнутся быстро.
 *  3. Отправляем "PROBE\n" успешно подключившимся и читаем "SERVER:имя".
 */
static void scan_network(void)
{
  char subnet[12]; /* "255.255.255\0" — первые три октета IPv4 */
  get_subnet(subnet, sizeof(subnet));
  printf("Сканирование %s.1–%s.254 (порт %d)...\n", subnet, subnet, PORT);
  srv_cnt = 0;

  int fds[SCAN_NUM];
  char ips[SCAN_NUM][20]; /* "255.255.255.254\0" = 16, 20 с запасом */
  struct pollfd pfd[SCAN_NUM];

  /* Инициируем все соединения параллельно */
  for (int i = 0; i < SCAN_NUM; i++)
  {
    snprintf(ips[i], sizeof(ips[i]), "%s.%d", subnet, i + 1);
    fds[i] = socket(AF_INET, SOCK_STREAM, 0);
    if (fds[i] < 0)
    {
      pfd[i].fd = -1;
      continue;
    }

    int fl = fcntl(fds[i], F_GETFL, 0);
    fcntl(fds[i], F_SETFL, fl | O_NONBLOCK);

    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(PORT);
    if (inet_pton(AF_INET, ips[i], &a.sin_addr) != 1)
    {
      close(fds[i]);
      fds[i] = -1;
      pfd[i].fd = -1;
      continue;
    }
    (void)connect(fds[i], (struct sockaddr *)&a,
                  sizeof(a)); /* EINPROGRESS — ок */

    pfd[i].fd = fds[i];
    pfd[i].events = POLLOUT;
    pfd[i].revents = 0;
  }

  /* Ждём 500 мс для всех соединений */
  poll(pfd, SCAN_NUM, 500);

  /* Зондируем те хосты, которые ответили */
  for (int i = 0; i < SCAN_NUM && srv_cnt < MAX_SRVS; i++)
  {
    if (fds[i] < 0 || !(pfd[i].revents & POLLOUT))
    {
      if (fds[i] >= 0)
        close(fds[i]);
      continue;
    }
    /* Проверяем, успешно ли соединение */
    int err;
    socklen_t el = sizeof(err);
    getsockopt(fds[i], SOL_SOCKET, SO_ERROR, &err, &el);
    if (err)
    {
      close(fds[i]);
      continue;
    }

    /* Переводим в блокирующий режим */
    int fl = fcntl(fds[i], F_GETFL, 0);
    fcntl(fds[i], F_SETFL, fl & ~O_NONBLOCK);

    /* Отправляем зонд и ждём ответа (до 1 с) */
    send(fds[i], "PROBE\n", 6, 0);
    struct pollfd p = {fds[i], POLLIN, 0};
    if (poll(&p, 1, 1000) <= 0 || !(p.revents & POLLIN))
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

    char *found = strstr(buf, "SERVER:");
    if (!found)
      continue;
    found += 7;
    char *end = strchr(found, '\n');
    if (end)
      *end = '\0';

    strncpy(srvs[srv_cnt].ip, ips[i], 31);
    strncpy(srvs[srv_cnt].name, found, NAME_LEN - 1);
    printf("  [%d] %s  (%s)\n", srv_cnt + 1, found, ips[i]);
    srv_cnt++;
  }

  if (!srv_cnt)
    puts("Серверов не найдено.");
}

/* ── Поток приёма сообщений ──────────────────────────────────────── */

static void *recv_thread(void *arg)
{
  (void)arg;
  char line[MSG_LEN];
  while (running)
  {
    int n = fd_readline(chat_fd, line, sizeof(line));
    if (n <= 0)
    {
      if (running)
      {
        printf("\n[Соединение с сервером разорвано]\n");
        fflush(stdout);
        running = 0;
      }
      break;
    }
    printf("%s", line);
    fflush(stdout);
  }
  return NULL;
}

/* ── Сессия чата ──────────────────────────────────────────────────── */

static void chat(int fd, const char *name)
{
  chat_fd = fd;
  running = 1;

  /* Читаем историю чата (до маркера END_HISTORY) */
  printf("\n=== История чата «%s» ===\n", name);
  char line[MSG_LEN];
  while (1)
  {
    int n = fd_readline(fd, line, sizeof(line));
    if (n <= 0)
    {
      puts("[Ошибка получения истории]");
      return;
    }
    if (strncmp(line, "END_HISTORY", 11) == 0)
      break;
    printf("%s", line);
  }
  printf("=== Начало чата. /quit — выход в меню ===\n");
  fflush(stdout);

  /* Запускаем поток приёма */
  pthread_t tid;
  pthread_create(&tid, NULL, recv_thread, NULL);

  /* Читаем ввод пользователя и отправляем */
  char input[MSG_LEN];
  while (running && fgets(input, sizeof(input), stdin))
  {
    strip_nl(input);
    if (!running)
      break;
    if (!*input)
      continue;

    char msg[MSG_LEN + 2];
    snprintf(msg, sizeof(msg), "%s\n", input);
    if (send(fd, msg, strlen(msg), 0) < 0)
      break;
    if (strcmp(input, "/quit") == 0)
    {
      running = 0;
      break;
    }
  }

  running = 0;
  shutdown(fd, SHUT_RDWR); /* будит поток приёма */
  pthread_join(tid, NULL);
}

/* ── Вспомогательные функции ──────────────────────────────────────── */

/* Устанавливает TCP-соединение с сервером. Возвращает fd или -1. */
static int connect_to(const char *ip)
{
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0)
  {
    perror("socket");
    return -1;
  }
  struct sockaddr_in a = {0};
  a.sin_family = AF_INET;
  a.sin_port = htons(PORT);
  if (inet_pton(AF_INET, ip, &a.sin_addr) != 1)
  {
    puts("Неверный IP-адрес.");
    close(fd);
    return -1;
  }
  if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0)
  {
    perror("connect");
    close(fd);
    return -1;
  }
  return fd;
}

/* Отправляет имя пользователя, читает имя сервера, запускает чат. */
static void do_chat(int fd, const char *fallback_name)
{
  /* Отправляем имя пользователя */
  char msg[NAME_LEN + 2];
  snprintf(msg, sizeof(msg), "%s\n", username);
  send(fd, msg, strlen(msg), 0);

  /* Читаем строку "SERVER:имя" */
  char line[MSG_LEN];
  char display_name[NAME_LEN];
  strncpy(display_name, fallback_name, NAME_LEN - 1);
  if (fd_readline(fd, line, sizeof(line)) > 0)
  {
    char *p = strstr(line, "SERVER:");
    if (p)
    {
      p += 7;
      strip_nl(p);
      strncpy(display_name, p, NAME_LEN - 1);
    }
  }

  chat(fd, display_name);
  close(fd);
  printf("\nОтключён от сервера.\n");
}

int main(void)
{
  printf("=== Чат-клиент ===\n");

  printf("Ваше имя: ");
  if (!fgets(username, sizeof(username), stdin))
    return 1;
  strip_nl(username);
  if (!*username)
  {
    printf("Имя не может быть пустым.\n");
    return 1;
  }

  while (1)
  {
    printf("\n[1] Найти серверы  [2] Ввести IP вручную  [0] Выход\n> ");

    char choice[4];
    if (!fgets(choice, sizeof(choice), stdin))
      break;

    if (choice[0] == '0')
      break;
    if (choice[0] == '1')
    {
      scan_network();
      if (!srv_cnt)
        continue;

      printf("Выберите сервер (0 — назад): ");
      fflush(stdout);
      char sel[8];
      if (!fgets(sel, sizeof(sel), stdin))
        break;
      int s = atoi(sel);
      if (s < 1 || s > srv_cnt)
        continue;

      int fd = connect_to(srvs[s - 1].ip);
      if (fd >= 0)
        do_chat(fd, srvs[s - 1].name);
    }
    else if (choice[0] == '2')
    {
      printf("IP сервера: ");
      char ip[32];
      if (!fgets(ip, sizeof(ip), stdin))
        continue;
      strip_nl(ip);

      int fd = connect_to(ip);
      if (fd >= 0)
        do_chat(fd, ip);
    }
  }

  return 0;
}
