#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include "RingBuffer.hpp"
#include "Common.hpp"

class Client {
public:
    Client(RingBuffer<ClientMessage>& buffer, int client_id, int message_cnt, int memory_space, AccessPattern pattern);
    void run();

private:
    RingBuffer<ClientMessage>& buffer_;
    int client_id_;
    int message_cnt_;
    MemoryAccessGenerator generator_;
};

#endif // CLIENT_H
