#pragma once

#include "protocol.h"

int send_notification(int socket_fd, Notification notification);
int parse_notification(char *data, Notification *notification);
void free_notification(Notification *notification);
