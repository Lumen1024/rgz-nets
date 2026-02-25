#pragma once

#include <vars.h>

typedef struct ServerInfo
{
    char ip[32];
    char name[NAME_LEN];
} ServerInfo;

int get_servers(ServerInfo *servers);