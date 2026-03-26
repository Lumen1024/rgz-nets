#pragma once

#include <protocol.h>

int connect_to_server(const char *host, int port);
Response send_and_wait(Request req);
