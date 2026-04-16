#pragma once

#include <protocol.h>
#include <cJSON.h>

typedef enum
{
    LIST_MODE_CHATS = 0,
    LIST_MODE_USERS = 1,
    LIST_MODE_MEMBERS = 2,
} ListMode;

typedef enum
{
    PANEL_NONE = -1,
    PANEL_CHAT = 0,
    PANEL_LIST = 1,
    PANEL_SYS = 2,
} Panel;


void message_from_json(cJSON *item, Message *out);
