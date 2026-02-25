#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vars.h>
#include <scanner.h>
#include <session.h>
#include <ui.h>

static char username[NAME_LEN];

static void menu_search()
{
    ServerInfo servers[MAX_SERVERS];
    int count = get_servers(servers);
    if (!count)
    {
        printf("Серверов не найдено.\n");
        return;
    }

    for (int i = 0; i < count; i++)
        printf("[%d] %s (%s)", i, servers[i].name, servers[i].ip);

    printf("\n\nВыберите сервер (0 — назад): ");
    int choice;
    if (getchoice(&choice, count))
        return;

    run_session(servers[choice].ip, username);
}

static void menu_manual()
{
    printf("IP сервера: ");
    char ip[32];
    if (!fgets(ip, sizeof(ip), stdin))
        return;
    ip[strcspn(ip, "\r\n")] = '\0';

    run_session(ip, username);
}

int main(void)
{
    printf("=== Чат-клиент ===\n");
    printf("Ваше имя: ");
    if (getusername(username) != 0)
        return 1;

    while (1)
    {
        printf("\n[1] Найти серверы  [2] Ввести IP вручную  [0] Выход\n> ");
        int choice;
        getchoice(&choice, 3);

        switch (choice)
        {
        case 0:
            return 0;
        case 1:
            menu_search();
            break;
        case 2:
            menu_manual();
            break;
        }
    }

    return 0;
}
