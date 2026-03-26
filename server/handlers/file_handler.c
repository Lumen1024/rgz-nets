#include "file_handler.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "cJSON.h"
#include "../../shared/protocol.h"
#include "../../shared/response.h"
#include "../repositories/user_repository.h"

// Simple in-memory file transfer tracking
#define MAX_FILE_TRANSFERS 256

typedef struct {
    char id[64];
    char from[MAX_LOGIN_LEN];
    char to[MAX_LOGIN_LEN];
    char filename[256];
    long size;
    int pending;    // 1 = waiting for approve/decline
    char sender_ip[64];
    int sender_port;
} FileTransfer;

static FileTransfer transfers[MAX_FILE_TRANSFERS];
static int transfer_count = 0;

static void generate_file_id(char *buf, size_t size) {
    snprintf(buf, size, "ft_%ld_%d", (long)time(NULL), transfer_count);
}

static FileTransfer *find_transfer(const char *file_id) {
    for (int i = 0; i < transfer_count; i++) {
        if (strcmp(transfers[i].id, file_id) == 0 && transfers[i].pending) {
            return &transfers[i];
        }
    }
    return NULL;
}

Response handle_file_request(const char *to, Request *req, const char *from) {
    if (!req->content) {
        return make_error(ERR_BAD_REQUEST);
    }

    cJSON *filename_j = cJSON_GetObjectItem(req->content, "filename");
    cJSON *size_j = cJSON_GetObjectItem(req->content, "size");

    if (!cJSON_IsString(filename_j) || !cJSON_IsNumber(size_j)) {
        return make_error(ERR_BAD_REQUEST);
    }

    if (!repo_user_exists(to)) {
        return make_error(ERR_NOT_FOUND);
    }

    if (transfer_count >= MAX_FILE_TRANSFERS) {
        return make_error(ERR_INTERNAL);
    }

    FileTransfer *ft = &transfers[transfer_count];
    generate_file_id(ft->id, sizeof(ft->id));
    strncpy(ft->from, from, MAX_LOGIN_LEN - 1);
    ft->from[MAX_LOGIN_LEN - 1] = '\0';
    strncpy(ft->to, to, MAX_LOGIN_LEN - 1);
    ft->to[MAX_LOGIN_LEN - 1] = '\0';
    strncpy(ft->filename, filename_j->valuestring, sizeof(ft->filename) - 1);
    ft->filename[sizeof(ft->filename) - 1] = '\0';
    ft->size = (long)size_j->valuedouble;
    ft->pending = 1;
    ft->sender_ip[0] = '\0';
    ft->sender_port = 0;
    transfer_count++;

    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "file_id", ft->id);

    return make_success(body);
}

Response handle_file_approve(const char *to, const char *file_id) {
    FileTransfer *ft = find_transfer(file_id);
    if (!ft) {
        return make_error(ERR_NOT_FOUND);
    }

    if (strcmp(ft->to, to) != 0) {
        return make_error(ERR_FORBIDDEN);
    }

    ft->pending = 0;

    // Return the sender's IP and port for P2P connection
    // sender_ip/port are filled in when the sender registers their address
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "ip", ft->sender_ip[0] ? ft->sender_ip : "0.0.0.0");
    cJSON_AddNumberToObject(body, "port", ft->sender_port);

    return make_success(body);
}

Response handle_file_decline(const char *to, const char *file_id) {
    FileTransfer *ft = find_transfer(file_id);
    if (!ft) {
        return make_error(ERR_NOT_FOUND);
    }

    if (strcmp(ft->to, to) != 0) {
        return make_error(ERR_FORBIDDEN);
    }

    ft->pending = 0;

    return make_success(NULL);
}
