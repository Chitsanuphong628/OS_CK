#ifndef COMMON_H
#define COMMON_H

#include <sys/types.h>

#define QUEUE_KEY 0x123456
#define NUM_RESOURCES 20
#define SERVER_MSG_TYPE 1

enum CommandType {
    CMD_LIST = 1,
    CMD_STATUS,
    CMD_RESERVE,
    CMD_CANCEL,
    CMD_QUIT
};

enum ResourceStatus {
    AVAILABLE = 0,
    RESERVED = 1
};

struct MessageBuffer {
    long mtype;
    int client_id;
    int command;
    int resource_id;
    char payload[512];
};

#endif
