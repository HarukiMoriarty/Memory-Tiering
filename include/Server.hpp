#ifndef SERVER_H
#define SERVER_H

#include <string>

#include "RingBuffer.hpp"
#include "PageTable.hpp"
#include "Scanner.hpp"
#include "Common.hpp"
#include "Utils.hpp"

class Server {
public:
    Server(RingBuffer<ClientMessage>& client_buffer, RingBuffer<MemMoveReq>& move_page_buffer_,
        const std::vector<size_t>& client_memory_spaces, const ServerMemoryConfig& server_config);
    ~Server();
    // Allocates memory in all three tiers and stores base addresses
    void allocateMemory(const ServerMemoryConfig& config);
    void handleClientMessage(const ClientMessage& msg);
    void handleMemoryMoveRequest(const MemMoveReq& req);
    void runManagerThread();
    void runPolicyThread();
    void start();

private:
    RingBuffer<ClientMessage>& client_buffer_;
    RingBuffer<MemMoveReq>& move_page_buffer_;
    std::vector<size_t> base_page_id_;
    PageTable* page_table_;
    Scanner* scanner_;
    ServerMemoryConfig server_config_;


    // Number of pages in each tier, stored for convenience
    size_t local_page_count_ = 0;
    size_t remote_page_count_ = 0;
    size_t pmem_page_count_ = 0;

    void* local_base_ = nullptr;
    void* remote_base_ = nullptr;
    void* pmem_base_ = nullptr;
};

#endif // SERVER_H
