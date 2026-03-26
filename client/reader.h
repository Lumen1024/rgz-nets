#pragma once

#include <protocol.h>

void *reader_thread(void *arg);
void reader_on_response(Response *res);
void reader_on_notification(Notification *notif);
