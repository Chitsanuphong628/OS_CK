#include "common.h"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

#include <sys/msg.h>

using namespace std;

bool parse_integer(const string& text, int& value) {
    istringstream input(text);
    string extra;

    if (!(input >> value) || (input >> extra)) {
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    int client_id = 0;
    if (argc != 2 || !parse_integer(argv[1], client_id) ||
        client_id <= 0 || client_id >= SERVER_MSG_TYPE) {
        cout << "Usage: " << argv[0] << " <client_id>\n"
             << "client_id must be from 1 to " << SERVER_MSG_TYPE - 1 << ".\n";
        return 1;
    }

    int message_queue = msgget(QUEUE_KEY, 0666);
    if (message_queue == -1) {
        perror("msgget: start the server first");
        return 1;
    }

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
            string extra;
            if (input >> extra) {
                cout << "QUIT does not take an argument.\n";
                continue;
            }
            cout << "Client-" << client_id << " disconnected.\n";
            break;
        } else {
            cout << "Unknown command. Use LIST, STATUS, RESERVE, CANCEL, or QUIT.\n";
            continue;
        }

        if (command == CMD_STATUS || command == CMD_RESERVE ||
            command == CMD_CANCEL) {
            string extra;
            if (!(input >> resource_id) || (input >> extra) ||
                resource_id < 1 || resource_id > NUM_RESOURCES) {
                cout << "Enter exactly one resource ID from 1 to "
                     << NUM_RESOURCES << ".\n";
                continue;
            }
        } else {
            string extra;
            if (input >> extra) {
                cout << command_text << " does not take an argument.\n";
                continue;
            }
        }

        MessageBuffer request{};
        request.mtype = SERVER_MSG_TYPE;
        request.client_id = client_id;
        request.command = command;
        request.resource_id = resource_id;

        const size_t message_size = sizeof(request) - sizeof(request.mtype);
        if (msgsnd(message_queue, &request, message_size, 0) == -1) {
            perror("msgsnd");
            return 1;
        }

        MessageBuffer response{};
        const ssize_t received = msgrcv(message_queue, &response, message_size,
                                        client_id, 0);
        if (received == -1) {
            perror("msgrcv");
            return 1;
        }
        if (received != static_cast<ssize_t>(message_size)) {
            cout << "Response size does not match common.h.\n";
            return 1;
        }

        response.payload[sizeof(response.payload) - 1] = '\0';
        if (response.client_id != client_id ||
            response.command != command ||
            response.resource_id != resource_id) {
            cout << "Response does not match the request.\n";
            return 1;
        }

        cout << "Server: " << response.payload << '\n';
    }

    return 0;
}
