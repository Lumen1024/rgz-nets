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

#define SERVER_PORT 8888      /* порт, на котором слушают серверы */
#define MAX_SERVERS 64        /* максимум серверов в списке найденных */
#define MAX_MESSAGE_SIZE 512  /* максимальная длина одного сообщения */
#define MAX_NAME_SIZE 64      /* максимальная длина имени сервера/пользователя */
#define SUBNET_HOST_COUNT 254 /* сканируем адреса .1 – .254 */

typedef struct
{
  char ip[32];
  char name[MAX_NAME_SIZE];
} Server;

static Server servers[MAX_SERVERS];    /* список найденных серверов */
static int server_count;               /* сколько серверов нашли */
static char username[MAX_NAME_SIZE];   /* имя текущего пользователя */
static volatile int is_session_active; /* флаг: идёт ли сеанс чата */
static int chat_socket;                /* сокет активного соединения с сервером */

// Читает одну строку из сокета посимвольно.
static int read_line_from_socket(int fd, char *buff, int max_size)
{
  for (int i = 0; i < max_size - 1; i++)
  {
    char ch;
    if (recv(fd, &ch, 1, 0) < 0)
      return -1;

    buff[i] = ch;

    if (ch == '\n')
    {
      buff[i + 1] = '\0';
      return i + 1;
    }
  }

  buff[max_size - 1] = '\0';
  return max_size - 1;
}

// Обрезает строку до /n
static void remove_newline(char *str)
{
  size_t end_index = strcspn(str, "\r\n");
  str[end_index] = '\0';
}

// возвращает подсеть первого подходящего интерфейса
static void get_subnet(char *out, size_t size)
{
  strncpy(out, "192.168.1", size - 1);

  struct ifaddrs *iface_list; // односвязный список интерфейсов
  if (getifaddrs(&iface_list) != 0)
    return;

  for (struct ifaddrs *iface = iface_list; iface; iface = iface->ifa_next)
  {
    // Интерфейсы без адреса и не-IPv4
    if (!iface->ifa_addr || iface->ifa_addr->sa_family != AF_INET)
      continue;

    // loopback (lo)
    if (strcmp(iface->ifa_name, "lo") == 0)
      continue;

    // Получаем IP в виде строки
    struct sockaddr_in *ipv4_addr = (struct sockaddr_in *)iface->ifa_addr;
    char ip_string[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ipv4_addr->sin_addr, ip_string, sizeof(ip_string));

    // Отрезаем последний октет
    char *last_dot = strrchr(ip_string, '.');
    size_t subnet_len = (size_t)(last_dot - ip_string);

    if (subnet_len >= size)
      subnet_len = size - 1;
    strncpy(out, ip_string, subnet_len);
    out[subnet_len] = '\0';

    break; /* берём только первый подходящий интерфейс */
  }

  freeifaddrs(iface_list);
}

/* ══════════════════════════════════════════════════════════════════════
 *  Сканирование сети
 * ══════════════════════════════════════════════════════════════════════
 *
 * Алгоритм:
 *   1. Открываем 254 неблокирующих сокета и запускаем connect() на всех
 *      сразу (параллельно).
 *   2. Ждём 500 мс — устройства в локальной сети успеют ответить.
 *   3. Тем, кто подключился, отправляем запрос "PROBE\n" и читаем ответ
 *      "SERVER:имя_сервера".
 */
static void scan_local_network(void)
{
  char subnet[12]; /* первые три октета, например "192.168.1" */
  get_subnet(subnet, sizeof(subnet));
  printf("Сканирование %s.1–%s.254 (порт %d)...\n", subnet, subnet, SERVER_PORT);
  server_count = 0;

  int sockets[SUBNET_HOST_COUNT];             /* сокет для каждого хоста */
  char ip_list[SUBNET_HOST_COUNT][20];        /* IP каждого хоста */
  struct pollfd poll_list[SUBNET_HOST_COUNT]; /* список для poll() */

  /* ── Шаг 1: открываем соединения ко всем 254 хостам параллельно ── */
  for (int i = 0; i < SUBNET_HOST_COUNT; i++)
  {
    snprintf(ip_list[i], sizeof(ip_list[i]), "%s.%d", subnet, i + 1);

    sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
    if (sockets[i] < 0)
    {
      poll_list[i].fd = -1; /* сокет не создан, пропускаем */
      continue;
    }

    /* Переводим сокет в неблокирующий режим, чтобы connect() не ждал */
    int flags = fcntl(sockets[i], F_GETFL, 0);
    fcntl(sockets[i], F_SETFL, flags | O_NONBLOCK);

    /* Указываем адрес и порт сервера */
    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, ip_list[i], &server_addr.sin_addr) != 1)
    {
      close(sockets[i]);
      sockets[i] = -1;
      poll_list[i].fd = -1;
      continue;
    }

    /* connect() вернёт EINPROGRESS — это нормально для неблокирующего сокета */
    connect(sockets[i], (struct sockaddr *)&server_addr, sizeof(server_addr));

    poll_list[i].fd = sockets[i];
    poll_list[i].events = POLLOUT; /* ждём готовности к записи = соединение установлено */
    poll_list[i].revents = 0;
  }

  /* ── Шаг 2: ждём 500 мс, пока хосты отвечают ── */
  poll(poll_list, SUBNET_HOST_COUNT, 500);

  /* ── Шаг 3: опрашиваем тех, кто ответил ── */
  for (int i = 0; i < SUBNET_HOST_COUNT && server_count < MAX_SERVERS; i++)
  {
    /* Пропускаем сокеты, которые не готовы */
    if (sockets[i] < 0 || !(poll_list[i].revents & POLLOUT))
    {
      if (sockets[i] >= 0)
        close(sockets[i]);
      continue;
    }

    /* Проверяем, не было ли ошибки при подключении */
    int connect_error = 0;
    socklen_t error_size = sizeof(connect_error);
    getsockopt(sockets[i], SOL_SOCKET, SO_ERROR, &connect_error, &error_size);
    if (connect_error)
    {
      close(sockets[i]);
      continue;
    }

    /* Возвращаем сокет в блокирующий режим для нормальной отправки/приёма */
    int flags = fcntl(sockets[i], F_GETFL, 0);
    fcntl(sockets[i], F_SETFL, flags & ~O_NONBLOCK);

    /* Отправляем запрос "PROBE\n" и ждём ответа не дольше 1 секунды */
    send(sockets[i], "PROBE\n", 6, 0);
    struct pollfd wait_for_response = {sockets[i], POLLIN, 0};
    if (poll(&wait_for_response, 1, 1000) <= 0 || !(wait_for_response.revents & POLLIN))
    {
      close(sockets[i]);
      continue;
    }

    /* Читаем ответ сервера */
    char response[256] = {0};
    int bytes_received = (int)recv(sockets[i], response, sizeof(response) - 1, 0);
    close(sockets[i]);
    if (bytes_received <= 0)
      continue;
    response[bytes_received] = '\0';

    /* Ищем "SERVER:" в ответе и извлекаем имя сервера */
    char *server_name = strstr(response, "SERVER:");
    if (!server_name)
      continue;
    server_name += 7; /* пропускаем "SERVER:" */
    char *name_end = strchr(server_name, '\n');
    if (name_end)
      *name_end = '\0';

    /* Сохраняем найденный сервер в список */
    strncpy(servers[server_count].ip, ip_list[i], 31);
    strncpy(servers[server_count].name, server_name, MAX_NAME_SIZE - 1);
    printf("  [%d] %s  (%s)\n", server_count + 1, server_name, ip_list[i]);
    server_count++;
  }

  if (!server_count)
    puts("Серверов не найдено.");
}

/* ══════════════════════════════════════════════════════════════════════
 *  Поток приёма сообщений
 * ══════════════════════════════════════════════════════════════════════ */

/*
 * Работает в отдельном потоке: читает сообщения от сервера и выводит их.
 * Завершается, когда сервер закрывает соединение или is_session_active = 0.
 */
static void *receive_messages_thread(void *unused)
{
  (void)unused;
  char received_line[MAX_MESSAGE_SIZE];

  while (is_session_active)
  {
    int bytes_read = read_line_from_socket(chat_socket, received_line, sizeof(received_line));
    if (bytes_read <= 0)
    {
      if (is_session_active)
      {
        printf("\n[Соединение с сервером разорвано]\n");
        is_session_active = 0;
      }
      break;
    }
    printf("%s", received_line);
  }

  return NULL;
}

/*
 * Ведёт диалог с сервером:
 *   - сначала читает историю сообщений,
 *   - затем запускает фоновый поток приёма,
 *   - и отправляет то, что вводит пользователь, пока он не напишет /quit.
 */
static void start_chat_session(int socket_fd, const char *server_name)
{
  chat_socket = socket_fd;
  is_session_active = 1;

  /* Читаем историю чата до специального маркера END_HISTORY */
  printf("\n=== История чата «%s» ===\n", server_name);
  char received_line[MAX_MESSAGE_SIZE];
  while (1)
  {
    int bytes_read = read_line_from_socket(socket_fd, received_line, sizeof(received_line));
    if (bytes_read <= 0)
    {
      puts("[Ошибка получения истории]");
      return;
    }
    if (strncmp(received_line, "END_HISTORY", 11) == 0)
      break;
    printf("%s", received_line);
  }
  printf("=== Начало чата. /quit — выход в меню ===\n");
  fflush(stdout);

  /* Запускаем фоновый поток, который будет выводить входящие сообщения */
  pthread_t receiver_thread;
  pthread_create(&receiver_thread, NULL, receive_messages_thread, NULL);

  /* Основной цикл: читаем ввод пользователя и отправляем на сервер */
  char user_input[MAX_MESSAGE_SIZE];
  while (is_session_active && fgets(user_input, sizeof(user_input), stdin))
  {
    remove_newline(user_input);

    if (!is_session_active)
      break;
    if (!*user_input) /* пустая строка — не отправляем */
      continue;

    /* Добавляем \n в конец и отправляем */
    char message_to_send[MAX_MESSAGE_SIZE + 2];
    snprintf(message_to_send, sizeof(message_to_send), "%s\n", user_input);
    if (send(socket_fd, message_to_send, strlen(message_to_send), 0) < 0)
      break;

    if (strcmp(user_input, "/quit") == 0)
    {
      is_session_active = 0;
      break;
    }
  }

  /* Завершаем сеанс: закрываем соединение и ждём фоновый поток */
  is_session_active = 0;
  shutdown(socket_fd, SHUT_RDWR); /* прерывает чтение в фоновом потоке */
  pthread_join(receiver_thread, NULL);
}

/* ══════════════════════════════════════════════════════════════════════
 *  Подключение к серверу
 * ══════════════════════════════════════════════════════════════════════ */

/*
 * Создаёт TCP-соединение с сервером по IP-адресу.
 * Возвращает файловый дескриптор сокета, или -1 при ошибке.
 */
static int connect_to_server(const char *ip_address)
{
  int new_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (new_socket < 0)
  {
    perror("socket");
    return -1;
  }

  struct sockaddr_in server_addr = {0};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);

  if (inet_pton(AF_INET, ip_address, &server_addr.sin_addr) != 1)
  {
    puts("Неверный IP-адрес.");
    close(new_socket);
    return -1;
  }

  if (connect(new_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("connect");
    close(new_socket);
    return -1;
  }

  return new_socket;
}

/*
 * После установки соединения:
 *   1. Отправляет имя пользователя.
 *   2. Читает строку "SERVER:имя" от сервера.
 *   3. Запускает сеанс чата.
 */
static void login_and_chat(int socket_fd, const char *fallback_name)
{
  /* Отправляем своё имя серверу */
  char login_message[MAX_NAME_SIZE + 2];
  snprintf(login_message, sizeof(login_message), "%s\n", username);
  send(socket_fd, login_message, strlen(login_message), 0);

  /* Читаем строку "SERVER:имя" и запоминаем имя для отображения */
  char server_response[MAX_MESSAGE_SIZE];
  char server_display_name[MAX_NAME_SIZE];
  strncpy(server_display_name, fallback_name, MAX_NAME_SIZE - 1);

  if (read_line_from_socket(socket_fd, server_response, sizeof(server_response)) > 0)
  {
    char *name_start = strstr(server_response, "SERVER:");
    if (name_start)
    {
      name_start += 7; /* пропускаем "SERVER:" */
      remove_newline(name_start);
      strncpy(server_display_name, name_start, MAX_NAME_SIZE - 1);
    }
  }

  start_chat_session(socket_fd, server_display_name);
  close(socket_fd);
  printf("\nОтключён от сервера.\n");
}

void menu_search()
{
  scan_local_network();
  if (!server_count)
    return;

  printf("Выберите сервер (0 — назад): ");

  char server_selection[8];
  if (!fgets(server_selection, sizeof(server_selection), stdin))
    return;

  int selected = atoi(server_selection);
  if (selected < 1 || selected > server_count)
    return;

  int socket_fd = connect_to_server(servers[selected - 1].ip);
  if (socket_fd >= 0)
    login_and_chat(socket_fd, servers[selected - 1].name);
}

void menu_manual()
{
  printf("IP сервера: ");
  char server_ip[32];
  if (!fgets(server_ip, sizeof(server_ip), stdin))
    return;
  remove_newline(server_ip);

  int socket_fd = connect_to_server(server_ip);
  if (socket_fd >= 0)
    login_and_chat(socket_fd, server_ip);
}

int main(void)
{
  printf("=== Чат-клиент ===\n");

  printf("Ваше имя: ");
  if (!fgets(username, sizeof(username), stdin))
    return 1;
  remove_newline(username);
  if (!*username)
  {
    printf("Имя не может быть пустым.\n");
    return 1;
  }

  while (1)
  {
    printf("\n[1] Найти серверы  [2] Ввести IP вручную  [0] Выход\n> ");
    char menu_choice[4];
    if (!fgets(menu_choice, sizeof(menu_choice), stdin))
      break;

    if (menu_choice[0] == '0')
      break;
    switch (menu_choice[0])
    {
    case '1':
      menu_search();
      break;
    case '2':
      menu_manual();
      break;
    }
  }

  return 0;
}