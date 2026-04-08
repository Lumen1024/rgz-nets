#pragma once

typedef enum
{
    LIST_MODE_CHATS   = 0,
    LIST_MODE_USERS   = 1,
    LIST_MODE_MEMBERS = 2,
} ListMode;

typedef enum
{
    PANEL_NONE = -1,
    PANEL_CHAT = 0,
    PANEL_LIST = 1,
    PANEL_SYS  = 2,
} Panel;

typedef enum
{
    SYS_IDLE = 0,
    SYS_WAIT_CONFIRM,
} SysState;
