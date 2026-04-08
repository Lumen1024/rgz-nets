#include <ui.h>
#include <input.h>
#include <api.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <locale.h>

int main(int argc, char *argv[])
{
    char host[64] = {0};
    int port = 0;
    char login[64] = {0};
    char password[64] = {0};
    int fast_login = 0;

    if (argc >= 3)
    {
        char addr[128];
        strncpy(addr, argv[1], sizeof(addr) - 1);
        char *colon = strrchr(addr, ':');
        if (colon)
        {
            *colon = '\0';
            strncpy(host, addr, sizeof(host) - 1);
            port = atoi(colon + 1);
        }
        else
        {
            strncpy(host, addr, sizeof(host) - 1);
            port = 8080;
        }

        strncpy(login, argv[2], sizeof(login) - 1);
        read_line("Password: ", password, sizeof(password), 1);
        fast_login = 1;
    }

    setlocale(LC_ALL, "");

    if (!fast_login)
        ui_prompt_server(host, &port);

    int fd = connect_to_server(host, port);
    if (fd < 0)
    {
        fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
        return 1;
    }

    if (fast_login)
    {
        if (api_login(login, password) != ERR_OK)
        {
            fprintf(stderr, "Login failed.\n");
            return 1;
        }
    }
    else
    {
        while (1)
        {
            printf("(1) Login  (2) Register\nChoice: ");
            fflush(stdout);
            char choice[8] = {0};
            if (!fgets(choice, sizeof(choice), stdin))
                break;

            int c = atoi(choice);

            if (c == 1)
            {
                ui_prompt_login(login, password);
                if (api_login(login, password) == ERR_OK)
                    break;
                printf("Login failed. Try again.\n");
            }
            else if (c == 2)
            {
                ui_prompt_register(login, password);
                if (api_register(login, password) == ERR_OK)
                    printf("Registered. Now log in.\n");
                else
                    printf("Registration failed.\n");
            }
        }
    }

    ui_init();
    ui_run();
    ui_destroy();

    return 0;
}
