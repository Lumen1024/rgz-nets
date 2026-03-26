#include <notify.h>

#include <stdlib.h>
#include <string.h>
#include <notification.h>
#include <chat_repository.h>

// Shared memory between parent and child processes is not available after fork,
// so notifications are sent by looking up the recipient's socket fd from a
// table written by the parent before forking.
// For simplicity this implementation writes the fd table to a shared file;
// a production server would use a pipe or shared memory segment instead.

#define MAX_CLIENTS 256

typedef struct {
    int  socket_fd;
    char login[MAX_LOGIN_LEN];
} ClientEntry;

static ClientEntry clients[MAX_CLIENTS];
static int client_count = 0;

void notify_register(int socket_fd, const char *login) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket_fd == socket_fd) {
            strncpy(clients[i].login, login, MAX_LOGIN_LEN - 1);
            clients[i].login[MAX_LOGIN_LEN - 1] = '\0';
            return;
        }
    }
    if (client_count >= MAX_CLIENTS) return;
    clients[client_count].socket_fd = socket_fd;
    strncpy(clients[client_count].login, login, MAX_LOGIN_LEN - 1);
    clients[client_count].login[MAX_LOGIN_LEN - 1] = '\0';
    client_count++;
}

void notify_unregister(int socket_fd) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket_fd == socket_fd) {
            clients[i] = clients[--client_count];
            return;
        }
    }
}

void notify_user(const char *login, Notification notif) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].login, login) == 0) {
            send_notification(clients[i].socket_fd, notif);
            return;
        }
    }
}

void notify_chat(const char *chat_name, Notification notif) {
    char **logins = NULL;
    int count = 0;
    if (repo_chat_list_users(chat_name, &logins, &count) != 0) return;

    for (int i = 0; i < count; i++) {
        notify_user(logins[i], notif);
        free(logins[i]);
    }
    free(logins);
}
