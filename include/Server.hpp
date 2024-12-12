#ifndef SERVER_H
#define SERVER_H

#include <string>

#include "RingBuffer.hpp"
#include "Common.hpp"

class Server {
public:
    Server(RingBuffer<Message>& client_buffer, RingBuffer<MemMoveReq>& move_page_buffer_, const std::vector<int>& client_memory_spaces);
    void runManagerThread();
    void runPolicyThread();
    void start();

private:
    RingBuffer<Message>& client_buffer_;
    RingBuffer<MemMoveReq>& move_page_buffer_;
    std::vector<int> base_addresses_;
};

#endif // SERVER_H
