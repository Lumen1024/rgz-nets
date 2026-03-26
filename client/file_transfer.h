#pragma once

#include <stddef.h>

typedef struct {
    char ip[64];
    int port;
    char filepath[512];
} FtSendArgs;

typedef struct {
    int port;
    char filename[256];
    size_t size;
    char save_path[512];
} FtReceiveArgs;

void *ft_send(void *args);
void *ft_receive(void *args);
