#pragma once

#include <notify/shared.h>
#include <notify/parent.h>
#include <notify/child.h>

// Must be called once in main() before any fork()
void notify_init();
