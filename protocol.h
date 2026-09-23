#ifndef OS_CK_PROTOCOL_H
#define OS_CK_PROTOCOL_H

#include <sys/ipc.h>

constexpr key_t REQUEST_QUEUE_KEY = 0x2231;
constexpr key_t REPLY_QUEUE_KEY = 0x2232;
constexpr long REQUEST_MESSAGE_TYPE = 1;

constexpr int RESOURCE_COUNT = 20;
constexpr int RESOURCE_AVAILABLE = 0;
constexpr int RESOURCE_RESERVED = 1;

constexpr int CMD_LIST = 1;
constexpr int CMD_STATUS = 2;
constexpr int CMD_RESERVE = 3;
constexpr int CMD_CANCEL = 4;
constexpr int CMD_QUIT = 5;

constexpr int RESPONSE_TEXT_SIZE = 2048;

struct RequestMessage {
    long mtype;
    int client_id;
    unsigned int request_id;
    int command;
    int resource_id;
};

struct ResponseMessage {
    long mtype;
    unsigned int request_id;
    int success;
    char text[RESPONSE_TEXT_SIZE];
};

#endif
