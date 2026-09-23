#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include <sys/ipc.h>
#include <sys/msg.h>

using namespace std;

const key_t REQUEST_QUEUE_KEY = 0x2231;
const key_t REPLY_QUEUE_KEY = 0x2232;
const int RESOURCE_COUNT = 20;

const int CMD_LIST = 1;
const int CMD_STATUS = 2;
const int CMD_RESERVE = 3;
const int CMD_CANCEL = 4;
const int CMD_QUIT = 5;

struct RequestMessage {
    long mtype;
    int client_id;
    int request_id;
    int command;
    int resource_id;
};

struct ResponseMessage {
    long mtype;
    int request_id;
    int success;
    char text[256];
};

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cout << "Usage: " << argv[0] << " <client_id>\n";
        return 1;
    }

    int client_id = atoi(argv[1]);
    if (client_id <= 0) {
        cout << "client_id must be a positive number.\n";
        return 1;
    }

    int request_queue = msgget(REQUEST_QUEUE_KEY, 0666);
    int reply_queue = msgget(REPLY_QUEUE_KEY, 0666);

    if (request_queue == -1 || reply_queue == -1) {
        perror("msgget: start the server first");
        return 1;
    }

    int request_id = 1;
    string line;

    while (true) {
        cout << "Client-" << client_id << "> ";
        if (!getline(cin, line)) {
            break;
        }

        stringstream input(line);
        string command_text;
        if (!(input >> command_text)) {
            continue;
        }

        int command = 0;
        int resource_id = 0;

        if (command_text == "LIST") {
            command = CMD_LIST;
        } else if (command_text == "STATUS") {
            command = CMD_STATUS;
        } else if (command_text == "RESERVE") {
            command = CMD_RESERVE;
        } else if (command_text == "CANCEL") {
            command = CMD_CANCEL;
        } else if (command_text == "QUIT") {
            command = CMD_QUIT;
        } else {
            cout << "Unknown command.\n";
            continue;
        }

        if (command == CMD_STATUS || command == CMD_RESERVE ||
            command == CMD_CANCEL) {
            string extra;
            if (!(input >> resource_id) || (input >> extra) ||
                resource_id < 1 || resource_id > RESOURCE_COUNT) {
                cout << "Enter one resource_id from 1 to "
                     << RESOURCE_COUNT << ".\n";
                continue;
            }
        }

        RequestMessage request{};
        request.mtype = 1;
        request.client_id = client_id;
        request.request_id = request_id;
        request.command = command;
        request.resource_id = resource_id;

        if (msgsnd(request_queue, &request,
                   sizeof(request) - sizeof(request.mtype), 0) == -1) {
            perror("msgsnd");
            return 1;
        }

        ResponseMessage response{};
        ssize_t received = msgrcv(reply_queue, &response,
                                  sizeof(response) - sizeof(response.mtype),
                                  client_id, 0);
        if (received == -1) {
            perror("msgrcv");
            return 1;
        }

        response.text[sizeof(response.text) - 1] = '\0';
        cout << (response.success ? "SUCCESS: " : "FAILED: ")
             << response.text << '\n';

        if (command == CMD_QUIT) {
            break;
        }

        request_id++;
    }

    return 0;
}
