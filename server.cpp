#include <iostream>
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

struct Resource {
    int id;
    int status;
    int owner_client_id;
};

int g_msg_queue_id = -1;
bool g_running = true;

void signal_handler(int signum) {
    std::cout << "\n[Server] Cleaning up Message Queue...\n";
    g_running = false;
    if (g_msg_queue_id != -1) {
        msgctl(g_msg_queue_id, IPC_RMID, nullptr);
    }
    exit(0);
}

class ReservationServer {
private:
    std::vector<Resource> resources;
    std::mutex table_mutex;
    int msg_queue_id;
    bool enable_sync;

    void random_delay() {
        static thread_local std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<int> distribution(50, 500);
        int delay_ms = distribution(generator);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    }

    bool handle_reserve(int worker_id, int client_id, int resource_id) {
        if (resource_id < 1 || resource_id > NUM_RESOURCES) return false;
        int idx = resource_id - 1;

        if (enable_sync) {
            table_mutex.lock();
            std::cout << "[Worker-" << worker_id << "] entering critical section\n";
        }

        bool success = false;
        std::cout << "[Worker-" << worker_id << "] check Resource " << resource_id 
                  << ": " << (resources[idx].status == AVAILABLE ? "AVAILABLE" : "RESERVED") << "\n";

        if (resources[idx].status == AVAILABLE) {
            random_delay();
            resources[idx].status = RESERVED;
            resources[idx].owner_client_id = client_id;
            success = true;
            std::cout << "[Worker-" << worker_id << "] Resource " << resource_id 
                      << " reserved by Client-" << client_id << "\n";
        } else {
            std::cout << "[Worker-" << worker_id << "] Resource " << resource_id 
                      << " already reserved\n";
        }

        if (enable_sync) {
            std::cout << "[Worker-" << worker_id << "] leaving critical section\n";
            table_mutex.unlock();
        }

        return success;
    }

    bool handle_cancel(int worker_id, int client_id, int resource_id) {
        if (resource_id < 1 || resource_id > NUM_RESOURCES) return false;
        int idx = resource_id - 1;

        if (enable_sync) {
            table_mutex.lock();
            std::cout << "[Worker-" << worker_id << "] entering critical section\n";
        }

        bool success = false;
        if (resources[idx].status == RESERVED && resources[idx].owner_client_id == client_id) {
            resources[idx].status = AVAILABLE;
            resources[idx].owner_client_id = -1;
            success = true;
            std::cout << "[Worker-" << worker_id << "] Resource " << resource_id 
                      << " cancelled by Client-" << client_id << "\n";
        } else {
            std::cout << "[Worker-" << worker_id << "] Resource " << resource_id 
                      << " cancel failed\n";
        }

        if (enable_sync) {
            std::cout << "[Worker-" << worker_id << "] leaving critical section\n";
            table_mutex.unlock();
        }

        return success;
    }

public:
    ReservationServer(int qid, bool sync_mode) : msg_queue_id(qid), enable_sync(sync_mode) {
        for (int i = 1; i <= NUM_RESOURCES; ++i) {
            resources.push_back({i, AVAILABLE, -1});
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
                    std::cout << "[Worker-" << worker_id << "] received RESERVE " << msg.resource_id 
                              << " from Client-" << msg.client_id << "\n";
                    bool ok = handle_reserve(worker_id, msg.client_id, msg.resource_id);
                    strcpy(response.payload, ok ? "SUCCESS" : "FAILED");
                    break;
                }
                case CMD_CANCEL: {
                    std::cout << "[Worker-" << worker_id << "] received CANCEL " << msg.resource_id 
                              << " from Client-" << msg.client_id << "\n";
                    bool ok = handle_cancel(worker_id, msg.client_id, msg.resource_id);
                    strcpy(response.payload, ok ? "SUCCESS" : "FAILED");
                    break;
                }
                case CMD_STATUS: {
                    std::cout << "[Worker-" << worker_id << "] received STATUS " << msg.resource_id 
                              << " from Client-" << msg.client_id << "\n";
                    int idx = msg.resource_id - 1;
                    if (idx >= 0 && idx < NUM_RESOURCES) {
                        if (resources[idx].status == AVAILABLE) {
                            strcpy(response.payload, "AVAILABLE");
                        } else {
                            std::string s = "RESERVED by Client-" + std::to_string(resources[idx].owner_client_id);
                            strcpy(response.payload, s.c_str());
                        }
                    } else {
                        strcpy(response.payload, "INVALID RESOURCE ID");
                    }
                    break;
                }
                case CMD_LIST: {
                    std::cout << "[Worker-" << worker_id << "] received LIST from Client-" << msg.client_id << "\n";
                    std::string list_str = "";
                    for (const auto& r : resources) {
                        list_str += "[" + std::to_string(r.id) + ": " + 
                                    (r.status == AVAILABLE ? "A" : "R") + "] ";
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
        std::cout << "========================================================\n";
        std::cout << " Server Started with " << num_workers << " Workers\n";
        std::cout << " Synchronization Mode: " << (enable_sync ? "ENABLED (Mutex)" : "DISABLED") << "\n";
        std::cout << "========================================================\n";

        std::vector<std::thread> workers;
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

    if (argc >= 2) num_workers = std::stoi(argv[1]);
    if (argc >= 3) enable_sync = (std::stoi(argv[2]) == 1);

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