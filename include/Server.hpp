#ifndef SERVER_H
#define SERVER_H

#include <string>

#include "RingBuffer.hpp"
#include "Common.h"

class Server {
public:
    Server(RingBuffer<Message>& buffer, const std::vector<int>& client_memory_spaces);
    void run();

private:
    RingBuffer<Message>& buffer_;
    std::vector<int> base_addresses_;
};

#endif // SERVER_H
