#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include "RingBuffer.hpp"
#include "Common.hpp"
#include "Generator.hpp"

class Client {
public:
    Client(RingBuffer<ClientMessage>& buffer, size_t client_id, size_t message_cnt, size_t memory_space_size, AccessPattern pattern, double rw_ratio);
    void run();

private:
    RingBuffer<ClientMessage>& buffer_;
    size_t client_id_;
    size_t message_cnt_;
    MemoryAccessGenerator generator_;
    double rw_ratio_;
};

#endif // CLIENT_H
