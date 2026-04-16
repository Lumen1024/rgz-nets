#pragma once

#include <ncurses.h>

#define SYS_BAR_H 3

typedef enum
{
    CP_DEFAULT = 1,
    CP_SELECTED = 2,
    CP_ACTIVE = 3,
    CP_NOTIFY = 4,
    CP_SYS = 5,
    CP_DIM = 6,
    CP_SYS_OK = 7,
    CP_SYS_ERR = 8,
} ColorPair;

typedef enum
{
    SYS_INFO = 0,
    SYS_OK,
    SYS_ERR,
} SysMsgType;
