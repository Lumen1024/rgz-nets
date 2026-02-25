#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vars.h>
#include <scanner.h>
#include <session.h>

static char username[NAME_LEN];

static void menu_search()
{
    ServerInfo servers[MAX_SERVERS];
    int count = scan_local_network(servers, MAX_SERVERS);
    if (!count)
    {
        printf("Серверов не найдено.\n");
        return;
    }

    printf("Выберите сервер (0 — назад): ");
    char buf[8];
    if (!fgets(buf, sizeof(buf), stdin))
        return;

    int sel = atoi(buf);
    if (sel < 1 || sel > count)
        return;

    session_run(servers[sel - 1].ip, servers[sel - 1].name, username);
}

static void menu_manual()
{
    printf("IP сервера: ");
    char ip[32];
    if (!fgets(ip, sizeof(ip), stdin))
        return;
    ip[strcspn(ip, "\r\n")] = '\0';

    session_run(ip, ip, username);
}

int main(void)
{
    printf("=== Чат-клиент ===\n");

    printf("Ваше имя: ");
    if (!fgets(username, sizeof(username), stdin))
        return 1;
    username[strcspn(username, "\r\n")] = '\0';
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
        switch (choice[0])
        {
        case '0':
            return 0;
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
