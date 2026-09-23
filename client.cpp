#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

#include <sys/ipc.h>
#include <sys/msg.h>

using namespace std;

namespace {

constexpr key_t kRequestQueueKey = static_cast<key_t>(0x2231);
constexpr key_t kReplyQueueKey = static_cast<key_t>(0x2232);
constexpr int32_t kResourceCount = 20;
constexpr size_t kReplyTextSize = 256;

struct RequestMessage {
    long mtype;
    int32_t client_id;
    uint32_t request_id;
    int32_t command;
    int32_t resource_id;
};

struct ResponseMessage {
    long mtype;
    uint32_t request_id;
    int32_t success;
    char text[kReplyTextSize];
};

enum Command : int32_t {
    CMD_LIST = 1,
    CMD_STATUS = 2,
    CMD_RESERVE = 3,
    CMD_CANCEL = 4,
    CMD_QUIT = 5,
};

bool parse_positive_integer(const string& text, long maximum, long& value) {
    try {
        size_t parsed_characters = 0;
        const long parsed = stol(text, &parsed_characters, 10);
        if (parsed_characters != text.size() || parsed <= 0 || parsed > maximum) {
            return false;
        }
        value = parsed;
        return true;
    } catch (const exception&) {
        return false;
    }
}

void print_help() {
    cout << "Commands:\n"
              << "  LIST\n"
              << "  STATUS <resource_id>\n"
              << "  RESERVE <resource_id>\n"
              << "  CANCEL <resource_id>\n"
              << "  QUIT\n"
              << "Resource IDs: 1-" << kResourceCount << "\n";
}

bool exchange_with_server(int request_queue_id,
                          int reply_queue_id,
                          int32_t client_id,
                          uint32_t request_id,
                          Command command,
                          int32_t resource_id) {
    RequestMessage request{};
    request.mtype = 1;
    request.client_id = client_id;
    request.request_id = request_id;
    request.command = command;
    request.resource_id = resource_id;

    const size_t request_size = sizeof(request) - sizeof(request.mtype);
    if (msgsnd(request_queue_id, &request, request_size, 0) == -1) {
        perror("msgsnd(request)");
        return false;
    }

    ResponseMessage response{};
    const size_t response_size = sizeof(response) - sizeof(response.mtype);
    const ssize_t received = msgrcv(reply_queue_id,
                                    &response,
                                    response_size,
                                    static_cast<long>(client_id),
                                    0);
    if (received == -1) {
        perror("msgrcv(reply)");
        return false;
    }
    if (static_cast<size_t>(received) != response_size) {
        cerr << "Reply message size does not match the client/server protocol.\n";
        return false;
    }

    response.text[kReplyTextSize - 1] = '\0';
    if (response.request_id != request_id) {
        cerr << "Reply request_id mismatch. Use a unique client ID and matching protocol.\n";
        return false;
    }

    cout << "[Client-" << client_id << "][Request-" << request_id << "] "
              << (response.success ? "SUCCESS: " : "FAILED: ")
              << response.text << '\n';
    return true;
}

bool parse_command(const string& line,
                   Command& command,
                   int32_t& resource_id) {
    istringstream input(line);
    string command_text;
    if (!(input >> command_text)) {
        return false;
    }

    resource_id = 0;
    if (command_text == "LIST" || command_text == "QUIT") {
        string extra;
        if (input >> extra) {
            cerr << command_text << " does not take an argument.\n";
            return false;
        }
        command = (command_text == "LIST") ? CMD_LIST : CMD_QUIT;
        return true;
    }

    if (command_text != "STATUS" && command_text != "RESERVE" &&
        command_text != "CANCEL") {
        cerr << "Unknown command. Type HELP to see available commands.\n";
        return false;
    }

    string resource_text;
    string extra;
    if (!(input >> resource_text) || (input >> extra)) {
        cerr << command_text << " requires exactly one resource_id.\n";
        return false;
    }

    long parsed_resource_id = 0;
    if (!parse_positive_integer(resource_text, kResourceCount, parsed_resource_id)) {
        cerr << "resource_id must be between 1 and " << kResourceCount << ".\n";
        return false;
    }
    resource_id = static_cast<int32_t>(parsed_resource_id);

    if (command_text == "STATUS") {
        command = CMD_STATUS;
    } else if (command_text == "RESERVE") {
        command = CMD_RESERVE;
    } else {
        command = CMD_CANCEL;
    }
    return true;
}

}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: " << argv[0] << " <client_id>\n"
                  << "Example: " << argv[0] << " 1\n";
        return 1;
    }

    long parsed_client_id = 0;
    if (!parse_positive_integer(argv[1], numeric_limits<int32_t>::max(),
                                parsed_client_id)) {
        cerr << "client_id must be a positive integer.\n";
        return 1;
    }
    const auto client_id = static_cast<int32_t>(parsed_client_id);

    const int request_queue_id = msgget(kRequestQueueKey, 0666);
    if (request_queue_id == -1) {
        perror("msgget(request queue) - is the server running?");
        return 1;
    }

    const int reply_queue_id = msgget(kReplyQueueKey, 0666);
    if (reply_queue_id == -1) {
        perror("msgget(reply queue) - is the server running?");
        return 1;
    }

    cout << "Connected as Client-" << client_id << ". Type HELP for commands.\n";
    uint32_t request_id = 1;
    string line;

    while (true) {
        cout << "Client-" << client_id << "> " << flush;
        if (!getline(cin, line)) {
            break;
        }

        if (line == "HELP") {
            print_help();
            continue;
        }

        Command command{};
        int32_t resource_id = 0;
        if (!parse_command(line, command, resource_id)) {
            continue;
        }

        if (!exchange_with_server(request_queue_id, reply_queue_id, client_id,
                                  request_id, command, resource_id)) {
            return 1;
        }

        if (command == CMD_QUIT) {
            break;
        }

        ++request_id;
        if (request_id == 0) {
            request_id = 1;
        }
    }

    cout << "Client-" << client_id << " disconnected.\n";
    return 0;
}
