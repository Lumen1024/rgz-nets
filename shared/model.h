#pragma once

#define MAX_LOGIN_LEN      64
#define MAX_TEXT_LEN     1024
#define MAX_TIMESTAMP_LEN  32
#define MAX_MESSAGES      512
#define MAX_CHATS         128
#define MAX_USERS         256
#define MAX_MEMBERS       256
#define MAX_SYS_MSG       512

typedef enum {
    ERR_OK           = 0,
    ERR_BAD_REQUEST  = 1,
    ERR_UNAUTHORIZED = 2,
    ERR_NOT_FOUND    = 3,
    ERR_FORBIDDEN    = 4,
    ERR_CONFLICT     = 5,
    ERR_INTERNAL     = 6,
} ErrorCode;

typedef enum {
    NOTIF_NEW_MESSAGE            = 1,
    NOTIF_FILE_REQUEST           = 2,
    NOTIF_FILE_APPROVED          = 3,
    NOTIF_FILE_DECLINED          = 4,
    NOTIF_CHAT_MEMBERS_CHANGED   = 5,
} NotifCode;

typedef struct {
    char login[MAX_LOGIN_LEN];
    char text[MAX_TEXT_LEN];
    char timestamp[MAX_TIMESTAMP_LEN];
} Message;
