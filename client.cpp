#include "protocol.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

#include <sys/msg.h>

using namespace std;

bool parse_client_id(const string& text, int& client_id) {
    istringstream input(text);
    string extra;

    if (!(input >> client_id) || (input >> extra) || client_id <= 0) {
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    int client_id = 0;
    if (argc != 2 || !parse_client_id(argv[1], client_id)) {
        cout << "Usage: " << argv[0] << " <positive_client_id>\n";
        return 1;
    }

    int request_queue = msgget(REQUEST_QUEUE_KEY, 0660);
    int reply_queue = msgget(REPLY_QUEUE_KEY, 0660);

    if (request_queue == -1 || reply_queue == -1) {
        perror("msgget: start the server first");
        return 1;
    }

    unsigned int request_id = 1;
    string line;

    while (true) {
        cout << "Client-" << client_id << "> ";
        if (!getline(cin, line)) {
            break;
        }

        istringstream input(line);
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
            cout << "Unknown command. Use LIST, STATUS, RESERVE, CANCEL, or QUIT.\n";
            continue;
        }

        string extra;
        if (command == CMD_STATUS || command == CMD_RESERVE ||
            command == CMD_CANCEL) {
            if (!(input >> resource_id) || (input >> extra) ||
                resource_id < 1 || resource_id > RESOURCE_COUNT) {
                cout << "Enter exactly one resource ID from 1 to "
                     << RESOURCE_COUNT << ".\n";
                continue;
            }
        } else if (input >> extra) {
            cout << command_text << " does not take an argument.\n";
            continue;
        }

        RequestMessage request{};
        request.mtype = REQUEST_MESSAGE_TYPE;
        request.client_id = client_id;
        request.request_id = request_id;
        request.command = command;
        request.resource_id = resource_id;

        const size_t request_size = sizeof(request) - sizeof(request.mtype);
        if (msgsnd(request_queue, &request, request_size, 0) == -1) {
            perror("msgsnd");
            return 1;
        }

        ResponseMessage response{};
        const size_t response_size = sizeof(response) - sizeof(response.mtype);
        const ssize_t received = msgrcv(reply_queue, &response, response_size,
                                        client_id, 0);
        if (received == -1) {
            perror("msgrcv");
            return 1;
        }
        if (received != static_cast<ssize_t>(response_size)) {
            cout << "Response size does not match protocol.h.\n";
            return 1;
        }

        response.text[sizeof(response.text) - 1] = '\0';
        if (response.request_id != request_id) {
            cout << "Response request ID does not match the request.\n";
            return 1;
        }

        cout << (response.success ? "SUCCESS: " : "FAILED: ")
             << response.text << '\n';

        if (command == CMD_QUIT) {
            break;
        }

        request_id++;
        if (request_id == 0) {
            request_id = 1;
        }
    }

    return 0;
}
