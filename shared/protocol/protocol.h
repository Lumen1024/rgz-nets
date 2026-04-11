#pragma once

#include <cJSON.h>
#include <model.h>

#define MAX_TOKEN_LEN 256
#define MAX_ROUTE_LEN 256
#define MSG_BUFFER_SIZE 8192
#define CHAT_ROUTE_LEN (MAX_ROUTE_LEN + 18)

typedef enum
{
    GET,
    POST,
    DELETE
} RequestType;

typedef enum
{
    MSG_REQUEST,
    MSG_RESPONSE,
    MSG_NOTIFICATION
} MessageKind;

typedef struct
{
    MessageKind kind;
    char *route;
    RequestType type;
    char *token;
    cJSON *content;
} Request;

typedef struct
{
    MessageKind kind;
    ErrorCode code;
    cJSON *content;
} Response;

typedef struct
{
    MessageKind kind;
    NotifCode code;
    cJSON *content;
} Notification;

int parse_message_kind(const char *buf, MessageKind *kind_out);
