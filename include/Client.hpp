#ifndef CLIENT_H
#define CLIENT_H

#include <string>

#include "RingBuffer.hpp"
#include "Common.h"

class Client {
public:
    Client(RingBuffer<Message>& buffer, int message_cnt);
    void run();

private:
    RingBuffer<Message>& buffer_;
    int message_cnt_;
};

#endif // CLIENT_H
