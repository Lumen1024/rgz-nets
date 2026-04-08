#include <reader.h>
#include <notification.h>
#include <response.h>
#include <socket_utils.h>
#include <protocol.h>

#include <stdlib.h>

void *reader_thread(void *arg)
{
    int fd = *(int *)arg;
    free(arg);

    char buffer[MSG_BUFFER_SIZE];

    while (read_message(fd, buffer, sizeof(buffer)) == 0)
    {
        MessageKind kind;
        if (parse_message_kind(buffer, &kind) != 0)
            continue;

        if (kind == MSG_RESPONSE)
        {
            Response res;
            if (parse_response(buffer, &res) == 0)
                reader_on_response(&res);
        }
        else if (kind == MSG_NOTIFICATION)
        {
            Notification notif;
            if (parse_notification(buffer, &notif) == 0)
            {
                handle_notification(&notif);
                free_notification(&notif);
            }
        }
    }

    return NULL;
}
