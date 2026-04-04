#include <connection.h>
#include <reader.h>
#include <request.h>
#include <response.h>
#include <socket_utils.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

static int g_socket_fd = -1;

// Shared state between connection and reader for send_and_wait
static pthread_mutex_t g_resp_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_resp_cond = PTHREAD_COND_INITIALIZER;
static Response g_pending_response;
static int g_response_ready = 0;

int connect_to_server(const char *host, int port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0)
    {
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        close(fd);
        return -1;
    }

    g_socket_fd = fd;

    pthread_t tid;
    int *fd_ptr = malloc(sizeof(int));
    *fd_ptr = fd;
    pthread_create(&tid, NULL, reader_thread, fd_ptr);
    pthread_detach(tid);

    return fd;
}

// Called by reader_thread when a response arrives
void reader_on_response(Response *res)
{
    pthread_mutex_lock(&g_resp_mutex);
    g_pending_response = *res;
    g_response_ready = 1;
    pthread_cond_signal(&g_resp_cond);
    pthread_mutex_unlock(&g_resp_mutex);
}

Response send_and_wait(Request req)
{
    pthread_mutex_lock(&g_resp_mutex);
    g_response_ready = 0;
    pthread_mutex_unlock(&g_resp_mutex);

    if (send_request(g_socket_fd, req) < 0)
    {
        Response err;
        err.kind = MSG_RESPONSE;
        err.code = ERR_INTERNAL;
        err.content = NULL;
        return err;
    }

    pthread_mutex_lock(&g_resp_mutex);
    while (!g_response_ready)
    {
        pthread_cond_wait(&g_resp_cond, &g_resp_mutex);
    }
    Response res = g_pending_response;
    pthread_mutex_unlock(&g_resp_mutex);

    return res;
}
