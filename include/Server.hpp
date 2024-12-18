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
        const std::vector<int>& client_memory_spaces, const ServerMemoryConfig& server_config);
    void handleClientMessage(const ClientMessage& msg);
    void handleMemoryMoveRequest(const MemMoveReq& req);
    void runManagerThread();
    void runPolicyThread();
    void start();

private:
    RingBuffer<ClientMessage>& client_buffer_;
    RingBuffer<MemMoveReq>& move_page_buffer_;
    std::vector<int> base_addresses_;
    PageTable* page_table_;
    Scanner* scanner_;
    ServerMemoryConfig server_config_;
};

#endif // SERVER_H
