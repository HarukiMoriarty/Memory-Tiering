#ifndef SERVER_H
#define SERVER_H

#include <boost/chrono.hpp>
#include <boost/thread/lock_guard.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp> 
#include <iostream>
#include <string>

#include "RingBuffer.hpp"
#include "PageTable.hpp"
#include "Scanner.hpp"
#include "Common.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"

class Server {
public:
    Server(RingBuffer<ClientMessage>& client_buffer, const std::vector<ClientConfig>& client_configs,
        const ServerMemoryConfig& server_config, const PolicyConfig& policy_config);
    ~Server();

    // Allocates memory in all three tiers and stores base addresses.
    void allocateMemory();

    // Generate random content for each page.
    void generateRandomContent();

    void handleClientMessage(const ClientMessage& msg);
    void runManagerThread();
    void runScannerThread();
    void start();

    // Shutdown function
    void signalShutdown();
    bool shouldShutdown();

private:
    RingBuffer<ClientMessage>& client_buffer_;
    PageTable* page_table_;
    Scanner* scanner_;

    ServerMemoryConfig server_config_;
    PolicyConfig policy_config_;
    std::vector<bool> client_done_flags_;

    // Base page id for each memory layer
    std::vector<size_t> base_page_id_;

    // Number of pages load in each tier
    size_t local_page_load_ = 0;
    size_t remote_page_load_ = 0;
    size_t pmem_page_load_ = 0;

    void* local_base_ = nullptr;
    void* remote_base_ = nullptr;
    void* pmem_base_ = nullptr;

    bool shutdown_flag_ = false;
    boost::mutex shutdown_mutex_;

};

#endif // SERVER_H
