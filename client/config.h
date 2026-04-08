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
} ColorPair;
