#ifndef CLIENT_H
#define CLIENT_H

#include "RingBuffer.hpp"
#include <string>

class Client {
public:
    Client(RingBuffer<std::string>& buffer, int message_cnt);
    void run();

private:
    RingBuffer<std::string>& buffer_;
    int message_cnt_;
};

#endif // CLIENT_H
