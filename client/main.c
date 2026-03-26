#include <ui.h>
#include <connection.h>
#include <actions.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    char host[64]      = {0};
    int  port          = 0;
    char login[64]     = {0};
    char password[64]  = {0};

    // 1. Ask for server address
    ui_prompt_server(host, &port);

    // 2. Connect
    int fd = connect_to_server(host, port);
    if (fd < 0) {
        fprintf(stderr, "Failed to connect to %s:%d\n", host, port);
        return 1;
    }

    // 3. Auth loop
    while (1) {
        printf("(1) Login  (2) Register\nChoice: ");
        fflush(stdout);
        char choice[8] = {0};
        if (!fgets(choice, sizeof(choice), stdin)) break;

        int c = atoi(choice);

        if (c == 1) {
            ui_prompt_login(login, password);
            AuthArgs args;
            strncpy(args.login,    login,    sizeof(args.login)    - 1);
            strncpy(args.password, password, sizeof(args.password) - 1);
            void *ret = action_login(&args);
            if ((intptr_t)ret == 0) break;
            printf("Login failed (code %d). Try again.\n", (int)(intptr_t)ret);
        } else if (c == 2) {
            ui_prompt_register(login, password);
            AuthArgs args;
            strncpy(args.login,    login,    sizeof(args.login)    - 1);
            strncpy(args.password, password, sizeof(args.password) - 1);
            void *ret = action_register(&args);
            if ((intptr_t)ret == 0) {
                printf("Registered. Now log in.\n");
            } else {
                printf("Registration failed (code %d).\n", (int)(intptr_t)ret);
            }
        }
    }

    // 4. Launch ncurses UI
    ui_init();
    ui_run();
    ui_destroy();

    return 0;
}
