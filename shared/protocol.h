#pragma once

#include <cJSON.h>

#define MAX_LOGIN_LEN 64
#define MAX_TEXT_LEN 1024
#define MAX_TIMESTAMP_LEN 32
#define MAX_TOKEN_LEN 256
#define MAX_ROUTE_LEN 256
#define MSG_BUFFER_SIZE 8192

typedef enum { GET, POST, DELETE } RequestType;
typedef enum { MSG_REQUEST, MSG_RESPONSE, MSG_NOTIFICATION } MessageKind;

typedef struct {
    MessageKind kind;
    char *route;
    RequestType type;
    char *token;
    cJSON *content;
} Request;

typedef struct {
    MessageKind kind;
    int code;
    cJSON *content;
} Response;

typedef struct {
    MessageKind kind;
    int code;
    cJSON *content;
} Notification;

typedef struct {
    char login[MAX_LOGIN_LEN];
    char text[MAX_TEXT_LEN];
    char timestamp[MAX_TIMESTAMP_LEN];
} Message;

// Error codes
#define ERR_OK           0
#define ERR_BAD_REQUEST  1
#define ERR_UNAUTHORIZED 2
#define ERR_NOT_FOUND    3
#define ERR_FORBIDDEN    4
#define ERR_CONFLICT     5
#define ERR_INTERNAL     6

// Notification codes
#define NOTIF_NEW_MESSAGE      1
#define NOTIF_FILE_REQUEST     2
#define NOTIF_FILE_APPROVED    3
#define NOTIF_FILE_DECLINED    4
