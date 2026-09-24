#include <iostream>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <random>
#include <cstring>
#include <csignal>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include "common.h"

using namespace std;

struct Resource {
    int id;
    int status;
    int owner_client_id;
};

int g_msg_queue_id = -1;
bool g_running = true;

void signal_handler(int signum) {
    cout << "\n[Server] Cleaning up Message Queue...\n";
    g_running = false;
    if (g_msg_queue_id != -1) {
        msgctl(g_msg_queue_id, IPC_RMID, nullptr);
    }
    exit(0);
}

class ReservationServer {
private:
    map<int, Resource> resources;
    mutex table_mutex;
    int msg_queue_id;
    bool enable_sync;

    void random_delay() {
        static thread_local mt19937 generator(random_device{}());
        uniform_int_distribution<int> distribution(50, 500);
        int delay_ms = distribution(generator);
        this_thread::sleep_for(chrono::milliseconds(delay_ms));
    }

    bool handle_reserve(int worker_id, int client_id, int resource_id) {
        if (resource_id < 1 || resource_id > NUM_RESOURCES) return false;
        Resource& resource = resources.at(resource_id);

        if (enable_sync) {
            table_mutex.lock();
            cout << "[Worker-" << worker_id << "] entering critical section\n";
        }

        bool success = false;
        cout << "[Worker-" << worker_id << "] check Resource " << resource_id
                  << ": " << (resource.status == AVAILABLE ? "AVAILABLE" : "RESERVED") << "\n";

        if (resource.status == AVAILABLE) {
            random_delay();
            resource.status = RESERVED;
            resource.owner_client_id = client_id;
            success = true;
            cout << "[Worker-" << worker_id << "] Resource " << resource_id
                      << " reserved by Client-" << client_id << "\n";
        } else {
            cout << "[Worker-" << worker_id << "] Resource " << resource_id
                      << " already reserved\n";
        }

        if (enable_sync) {
            cout << "[Worker-" << worker_id << "] leaving critical section\n";
            table_mutex.unlock();
        }

        return success;
    }

    bool handle_cancel(int worker_id, int client_id, int resource_id) {
        if (resource_id < 1 || resource_id > NUM_RESOURCES) return false;
        Resource& resource = resources.at(resource_id);

        if (enable_sync) {
            table_mutex.lock();
            cout << "[Worker-" << worker_id << "] entering critical section\n";
        }

        bool success = false;
        if (resource.status == RESERVED && resource.owner_client_id == client_id) {
            resource.status = AVAILABLE;
            resource.owner_client_id = -1;
            success = true;
            cout << "[Worker-" << worker_id << "] Resource " << resource_id
                      << " cancelled by Client-" << client_id << "\n";
        } else {
            cout << "[Worker-" << worker_id << "] Resource " << resource_id
                      << " cancel failed\n";
        }

        if (enable_sync) {
            cout << "[Worker-" << worker_id << "] leaving critical section\n";
            table_mutex.unlock();
        }

        return success;
    }

public:
    ReservationServer(int qid, bool sync_mode) : msg_queue_id(qid), enable_sync(sync_mode) {
        for (int i = 1; i <= NUM_RESOURCES; ++i) {
            resources.emplace(i, Resource{i, AVAILABLE, -1});
        }
    }

    void worker_loop(int worker_id) {
        MessageBuffer msg;
        while (g_running) {
            ssize_t bytes = msgrcv(msg_queue_id, &msg, sizeof(MessageBuffer) - sizeof(long), SERVER_MSG_TYPE, 0);
            if (bytes == -1) {
                if (!g_running) break;
                continue;
            }

            MessageBuffer response;
            response.mtype = msg.client_id;
            response.client_id = msg.client_id;
            response.command = msg.command;
            response.resource_id = msg.resource_id;

            switch (msg.command) {
                case CMD_RESERVE: {
                    cout << "[Worker-" << worker_id << "] received RESERVE " << msg.resource_id
                              << " from Client-" << msg.client_id << "\n";
                    bool ok = handle_reserve(worker_id, msg.client_id, msg.resource_id);
                    strcpy(response.payload, ok ? "SUCCESS" : "FAILED");
                    break;
                }
                case CMD_CANCEL: {
                    cout << "[Worker-" << worker_id << "] received CANCEL " << msg.resource_id
                              << " from Client-" << msg.client_id << "\n";
                    bool ok = handle_cancel(worker_id, msg.client_id, msg.resource_id);
                    strcpy(response.payload, ok ? "SUCCESS" : "FAILED");
                    break;
                }
                case CMD_STATUS: {
                    cout << "[Worker-" << worker_id << "] received STATUS " << msg.resource_id
                              << " from Client-" << msg.client_id << "\n";
                    auto resource = resources.find(msg.resource_id);
                    if (resource != resources.end()) {
                        if (resource->second.status == AVAILABLE) {
                            strcpy(response.payload, "AVAILABLE");
                        } else {
                            string s = "RESERVED by Client-" + to_string(resource->second.owner_client_id);
                            strcpy(response.payload, s.c_str());
                        }
                    } else {
                        strcpy(response.payload, "INVALID RESOURCE ID");
                    }
                    break;
                }
                case CMD_LIST: {
                    cout << "[Worker-" << worker_id << "] received LIST from Client-" << msg.client_id << "\n";
                    string list_str = "";
                    for (const auto& entry : resources) {
                        const Resource& resource = entry.second;
                        list_str += "[" + to_string(resource.id) + ": " +
                                    (resource.status == AVAILABLE ? "A" : "R") + "] ";
                    }
                    strncpy(response.payload, list_str.c_str(), sizeof(response.payload) - 1);
                    break;
                }
                default:
                    strcpy(response.payload, "UNKNOWN_COMMAND");
                    break;
            }

            msgsnd(msg_queue_id, &response, sizeof(MessageBuffer) - sizeof(long), 0);
        }
    }

    void run(int num_workers) {
        cout << "========================================================\n";
        cout << " Server Started with " << num_workers << " Workers\n";
        cout << " Synchronization Mode: " << (enable_sync ? "ENABLED (Mutex)" : "DISABLED") << "\n";
        cout << "========================================================\n";

        vector<thread> workers;
        for (int i = 1; i <= num_workers; ++i) {
            workers.emplace_back(&ReservationServer::worker_loop, this, i);
        }

        for (auto &w : workers) {
            if (w.joinable()) w.join();
        }
    }
};

int main(int argc, char* argv[]) {
    int num_workers = 3;
    bool enable_sync = false;

    if (argc >= 2) num_workers = stoi(argv[1]);
    if (argc >= 3) enable_sync = (stoi(argv[2]) == 1);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    g_msg_queue_id = msgget(QUEUE_KEY, IPC_CREAT | 0666);
    if (g_msg_queue_id == -1) {
        perror("Failed to create message queue");
        return 1;
    }

    ReservationServer server(g_msg_queue_id, enable_sync);
    server.run(num_workers);

    return 0;
}
