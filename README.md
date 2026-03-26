# Чат по локальной сети

Разработка сетевого приложения «Чат». Мультипроцессная реализация сервера с установлением соединения с использованием функции fork

Авторизация, приватные сообщение, передача файлов между двумя клиентами в отдельном TCP соединении (минуя сервер).

---

**Требования**
- Язык си + OS Linux
- Использование низкоуровневого api (socket, bind, getsocketname и т.д.)
- Использование протокола TCP
- Обработка множественных клиентов через fork
- Авторизация
- Приватные и групповые сообщения
- Передача файлов между 2 клиентами в отдельном соединении минуя сервер

**Стиль кода**
- Защита заголовочных файлов — `#pragma once`
- Комментарии — только `//`, не `/* */`
- Все заголовки через `<>` (пути прописаны в Makefile через `-I`)

---

**Структура Проекта**
- client - папка с кодом клиентского приложения
- server - папка с кодом серверного приложения
- shared - папка с общим кодом и вспомогательными функциями
- data - папка для файлов в которых будут храниться данные во время работы приложения
- bin - папка для исполняемых файлов
- lib - папка для сторонних библиотек (например cJSON)

---

**Сборка**
У проекта есть общий Makefile, находящийся в корне, и локальные, которые находятся в папках.

Makefile должны быть универсальные, должны компилировать все .c файлы + main, причем поиск исходных файлов должен включать подпапки 

- server, client - компилируются в исполняемые файлы
- shared - в библиотеку

## Данные

**Хранение данных**
Все данные хранятся в Json файлах в папке data
- data/users.json - хранит в себе список имен пользователей с хешем пароля
- data/chats.json - список (имя чата, имя создателя, список пользователей)
- data/messages/chat/{chat_name}.json -  список сообщений чата
- data/messages/private/{user_name}-{user_name}.json - список личных сообщений

---

**Доступ к данным**
Чтобы во время работы сервер мог получить доступ к данным в json файлах, должны быть реализованы файлы репозиториев:
- user_repository.h - CRUD операции по пользователям
- chat_repository.h - CRUD операции по групповым чатам
- messages_repositor.h - CRUD по приватным и групповым сообщениям


# Клиент-серверное взаимодействие

Обмен данными между сервером и клиентом происходит по TCP. Каждый запрос и ответ — JSON строка. Клиент отправляет `Request`, сервер отвечает `Response`. Сервер также может сам рассылать `Notification` всем подключённым клиентам.

## API

### Сервер и авторизация

| Метод    | Маршрут     | Описание                          | Тело запроса        | Тело ответа                       | Auth |
| :------- | :---------- | :-------------------------------- | :------------------ | :-------------------------------- | :--: |
| **GET**  | `/info`     | Информация о сервере              | —                   | `{name, users_count, chat_count}` | —    |
| **POST** | `/register` | Регистрация нового пользователя   | `{login, password}` | —                                 | —    |
| **POST** | `/login`    | Вход и получение токена           | `{login, password}` | `{token}`                         | —    |

### Пользователи

| Метод   | Маршрут    | Описание                          | Тело запроса | Тело ответа    | Auth |
| :------ | :--------- | :-------------------------------- | :----------- | :------------- | :--: |
| **GET** | `/users`   | Список всех пользователей         | —            | `List<String>` | +    |

### Чаты

| Метод      | Маршрут              | Описание                             | Тело запроса | Тело ответа    | Auth |
| :--------- | :------------------- | :----------------------------------- | :----------- | :------------- | :--: |
| **GET**    | `/chats`             | Чаты текущего пользователя           | —            | `List<String>` | +    |
| **POST**   | `/chats`             | Создание нового чата                 | `{name}`     | —              | +    |
| **DELETE** | `/chats/{name}`      | Удаление чата (только для создателя) | —            | —              | +    |
| **GET**    | `/chats/{name}/host` | Получение создателя чата             | —            | `{login}`      | +    |

### Участники чата

| Метод      | Маршрут               | Описание                       | Тело запроса | Тело ответа    | Auth |
| :--------- | :-------------------- | :----------------------------- | :----------- | :------------- | :--: |
| **GET**    | `/chats/{name}/users` | Список участников чата         | —            | `List<String>` | +    |
| **POST**   | `/chats/{name}/users` | Добавить пользователя в чат    | `{login}`    | —              | +    |
| **DELETE** | `/chats/{name}/users` | Удалить пользователя из чата   | `{login}`    | —              | +    |

### Сообщения

| Метод    | Маршрут                      | Описание                            | Тело запроса | Тело ответа                       | Auth |
| :------- | :--------------------------- | :---------------------------------- | :----------- | :-------------------------------- | :--: |
| **GET**  | `/chats/{name}/messages`     | Все сообщения группового чата       | —            | `List<{login, text, timestamp}>`  | +    |
| **POST** | `/chats/{name}/messages`     | Отправить сообщение в группу        | `{text}`     | —                                 | +    |
| **GET**  | `/users/{login}/messages`    | Личный диалог с пользователем       | —            | `List<{login, text, timestamp}>`  | +    |
| **POST** | `/users/{login}/messages`    | Отправить личное сообщение          | `{text}`     | —                                 | +    |

### Передача файлов (P2P)

| Метод    | Маршрут                              | Описание                                            | Тело запроса         | Тело ответа  | Auth |
| :------- | :----------------------------------- | :-------------------------------------------------- | :------------------- | :----------- | :--: |
| **POST** | `/users/{login}/files`               | Запрос на отправку файла получателю                 | `{filename, size}`   | `{file_id}`  | +    |
| **POST** | `/users/{login}/files/{id}/approve`  | Получатель подтверждает приём (сервер отдаёт адрес) | —                    | `{ip, port}` | +    |
| **POST** | `/users/{login}/files/{id}/decline`  | Получатель отклоняет передачу                       | —                    | —            | +    |

---

## Форматы сообщений

Все три типа сообщений содержат поле `kind` для различения на стороне клиента (reader thread):

| Значение `kind`  | Тип сообщения  |
| :--------------- | :------------- |
| `"request"`      | Request        |
| `"response"`     | Response       |
| `"notification"` | Notification   |

### Request

| Поле    | Тип    | Описание                                                                                |
| :------ | :----- | :-------------------------------------------------------------------------------------- |
| kind    | string | Всегда `"request"`                                                                      |
| Route   | string | Маршрут ресурса (например `/chats/general/messages`)                                    |
| Type    | enum   | Тип запроса: `GET`, `POST`, `DELETE`                                                    |
| Headers | map    | Ключ-значение — используется для передачи токена авторизации (`Authorization: <token>`) |
| Content | JSON   | Тело запроса в формате cJSON                                                            |

### Response

| Поле    | Тип    | Описание                                          |
| :------ | :----- | :------------------------------------------------ |
| kind    | string | Всегда `"response"`                               |
| Code    | int    | `0` — успешно, остальное — коды ошибок            |
| Content | JSON   | Тело ответа в формате cJSON                       |

### Notification

| Поле    | Тип    | Описание                                          |
| :------ | :----- | :------------------------------------------------ |
| kind    | string | Всегда `"notification"`                           |
| Code    | int    | Тип уведомления                                   |
| Content | JSON   | Данные уведомления в формате cJSON                |

---

# Файлы проекта

## shared/

### `shared/protocol.h`
Определения структур и перечислений протокола, используемых и сервером, и клиентом.
```
typedef enum { GET, POST, DELETE } RequestType;
typedef enum { MSG_REQUEST, MSG_RESPONSE, MSG_NOTIFICATION } MessageKind;

typedef struct { ... } Request;
typedef struct { ... } Response;
typedef struct { ... } Notification;
```

### `shared/request.h` / `request.c`

- `int send_request(int socket_fd, Request request)` — сериализует и отправляет запрос
- `int parse_request(char *data, Request *request)` — десериализует запрос из строки
- `void free_request(Request *request)` — освобождает память запроса

### `shared/response.h` / `response.c`

- `int send_response(int socket_fd, Response response)` — сериализует и отправляет ответ
- `int parse_response(char *data, Response *response)` — десериализует ответ из строки
- `void free_response(Response *response)` — освобождает память ответа
- `Response make_error(int code)` — создаёт ответ-ошибку без тела
- `Response make_success(cJSON *content)` — создаёт успешный ответ с телом

### `shared/notification.h` / `notification.c`

- `int send_notification(int socket_fd, Notification notification)` — отправляет уведомление
- `int parse_notification(char *data, Notification *notification)` — десериализует уведомление
- `void free_notification(Notification *notification)` — освобождает память уведомления

### `shared/socket_utils.h` / `socket_utils.c`

- `int read_message(int socket_fd, char *buffer, size_t max_len)` — читает одно сообщение целиком (с учётом фрагментации TCP)
- `int write_message(int socket_fd, const char *data)` — отправляет строку с гарантией полной записи
- `int get_peer_ip(int socket_fd, char *ip_out)` — получает IP адрес удалённой стороны

### `shared/auth.h` / `auth.c`
Хеширование паролей и работа с токенами.
- `char *hash_password(const char *password)` — возвращает хеш пароля
- `int verify_password(const char *password, const char *hash)` — сверяет пароль с хешем
- `char *generate_token(const char *login)` — генерирует токен сессии для пользователя
- `int validate_token(const char *token, char *login_out)` — проверяет токен и возвращает логин

---

## server/

### `server/client_handler.h` / `client_handler.c`
Логика обработки одного клиента (выполняется в дочернем процессе после fork).
- `void handle_client(int socket_fd)` — главный цикл чтения запросов от клиента, маршрутизации и отправки ответов

### `server/handlers/auth_handler.h` / `auth_handler.c`
Обработчики маршрутов `/register`, `/login`, `/info`.
- `Response handle_register(Request *req)` — регистрация пользователя
- `Response handle_login(Request *req)` — вход, возврат токена
- `Response handle_info(Request *req)` — информация о сервере

### `server/handlers/chat_handler.h` / `chat_handler.c`
Обработчики маршрутов `/chats`.
- `Response handle_get_chats(const char *login)` — список чатов пользователя
- `Response handle_create_chat(Request *req, const char *login)` — создание чата
- `Response handle_delete_chat(const char *name, const char *login)` — удаление чата
- `Response handle_get_chat_host(const char *name)` — получение создателя чата
- `Response handle_get_chat_users(const char *name)` — список участников
- `Response handle_add_chat_user(const char *name, Request *req)` — добавить участника
- `Response handle_remove_chat_user(const char *name, Request *req)` — удалить участника

### `server/handlers/message_handler.h` / `message_handler.c`
Обработчики маршрутов `/chats/{name}/messages` и `/users/{login}/messages`.
- `Response handle_get_chat_messages(const char *chat)` — сообщения группового чата
- `Response handle_post_chat_message(const char *chat, Request *req, const char *login)` — отправить в группу
- `Response handle_get_private_messages(const char *login_a, const char *login_b)` — личный диалог
- `Response handle_post_private_message(const char *to, Request *req, const char *login)` — отправить лично

### `server/handlers/file_handler.h` / `file_handler.c`

- `Response handle_file_request(const char *to, Request *req, const char *from)` — инициировать передачу файла
- `Response handle_file_approve(const char *to, const char *file_id)` — подтвердить приём, отдать ip:port
- `Response handle_file_decline(const char *to, const char *file_id)` — отклонить передачу

### `server/notify.h` / `notify.c`

- `void notify_chat(const char *chat_name, Notification notif)` — уведомить всех участников чата
- `void notify_user(const char *login, Notification notif)` — уведомить конкретного пользователя

### `server/repositories/user_repository.h` / `user_repository.c`

- `int repo_user_exists(const char *login)` — проверить существование пользователя
- `int repo_user_create(const char *login, const char *password_hash)` — создать пользователя
- `int repo_user_get_hash(const char *login, char *hash_out)` — получить хеш пароля
- `int repo_user_list(char ***logins_out, int *count)` — список всех логинов

### `server/repositories/chat_repository.h` / `chat_repository.c`

- `int repo_chat_create(const char *name, const char *creator)` — создать чат
- `int repo_chat_delete(const char *name)` — удалить чат
- `int repo_chat_get_host(const char *name, char *host_out)` — получить создателя
- `int repo_chat_list_for_user(const char *login, char ***names_out, int *count)` — чаты пользователя
- `int repo_chat_add_user(const char *chat, const char *login)` — добавить участника
- `int repo_chat_remove_user(const char *chat, const char *login)` — удалить участника
- `int repo_chat_list_users(const char *chat, char ***logins_out, int *count)` — список участников

### `server/repositories/message_repository.h` / `message_repository.c`

- `int repo_msg_save_chat(const char *chat, const char *login, const char *text)` — сохранить сообщение в чат
- `int repo_msg_get_chat(const char *chat, Message **msgs_out, int *count)` — получить сообщения чата
- `int repo_msg_save_private(const char *from, const char *to, const char *text)` — сохранить личное сообщение
- `int repo_msg_get_private(const char *login_a, const char *login_b, Message **msgs_out, int *count)` — получить личный диалог

---

## client/

### `client/reader.h` / `reader.c`
Поток, постоянно читающий данные из сокета и маршрутизирующий их.
- `void *reader_thread(void *socket_fd)` — точка входа потока
- `void reader_on_response(Response *res)` — вызывается при получении response (сигналит condvar)
- `void reader_on_notification(Notification *notif)` — вызывается при получении notification (обновляет UI)

### `client/connection.h` / `connection.c`
Управление соединением и отправкой запросов.
- `int connect_to_server(const char *host, int port)` — устанавливает TCP соединение, возвращает socket_fd
- `Response send_and_wait(Request req)` — отправляет запрос и блокируется до получения ответа (через condvar)

### `client/ui.h` / `ui.c`
Отображение интерфейса в терминале и обработка ввода.
- `void ui_show_menu()` — вывести главное меню
- `void ui_show_chat(const char *chat_name)` — открыть экран чата
- `void ui_show_messages(Message *msgs, int count)` — вывести список сообщений
- `void ui_notify(const char *text)` — показать уведомление пользователю
- `void ui_refresh()` — перерисовать текущий экран

### `client/actions.h` / `actions.c`
Действия пользователя — каждое выполняется в отдельном потоке, вызывает `send_and_wait` и обновляет UI.
- `void *action_login(void *args)` — вход в систему
- `void *action_register(void *args)` — регистрация
- `void *action_send_message(void *args)` — отправить сообщение
- `void *action_create_chat(void *args)` — создать чат
- `void *action_send_file(void *args)` — инициировать передачу файла

### `client/file_transfer.h` / `file_transfer.c`
P2P передача файлов напрямую между клиентами.
- `void *ft_send(void *args)` — поток-отправитель: подключается к получателю, передаёт файл
- `void *ft_receive(void *args)` — поток-получатель: открывает порт, принимает файл
