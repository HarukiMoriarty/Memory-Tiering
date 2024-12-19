#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include "RingBuffer.hpp"
#include "Common.hpp"

class Client {
public:
    Client(RingBuffer<ClientMessage>& buffer, size_t client_id, size_t message_cnt, size_t memory_space, AccessPattern pattern);
    void run();

private:
    RingBuffer<ClientMessage>& buffer_;
    size_t client_id_;
    size_t message_cnt_;
    MemoryAccessGenerator generator_;
};

#endif // CLIENT_H
