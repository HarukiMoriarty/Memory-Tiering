#ifndef SERVER_H
#define SERVER_H

#include <string>

#include "RingBuffer.hpp"
#include "PageTable.hpp"
#include "Scanner.hpp"
#include "Common.hpp"
#include "Utils.hpp"
#include <boost/thread/mutex.hpp>
#include <boost/thread/lock_guard.hpp>

class Server {
public:
    Server(RingBuffer<ClientMessage>& client_buffer, const std::vector<ClientConfig>& client_configs,
        const ServerMemoryConfig& server_config, const PolicyConfig& policy_config);
    ~Server();

    // Allocates memory in all three tiers and stores base addresses
    void allocateMemory();
    void generateRandomContent();
    void handleClientMessage(const ClientMessage& msg);
    void runManagerThread();
    void runScannerThread();
    void start();

    // Shutdown function
    void signalShutdown();  // Sets the shutdown flag
    bool shouldShutdown();  // Checks the shutdown flag

private:
    RingBuffer<ClientMessage>& client_buffer_;

    std::vector<size_t> base_page_id_;
    PageTable* page_table_;
    Scanner* scanner_;

    ServerMemoryConfig server_config_;
    PolicyConfig policy_config_;
    std::vector<bool> client_done_flags_;

    // Number of pages load in each tier
    size_t local_page_load_ = 0;
    size_t remote_page_load_ = 0;
    size_t pmem_page_load_ = 0;

    void* local_base_ = nullptr;
    void* remote_base_ = nullptr;
    void* pmem_base_ = nullptr;

    bool shutdown_flag_ = false;    // Shared shutdown flag
    boost::mutex shutdown_mutex_;   // Protects the flag

};

#endif // SERVER_H
