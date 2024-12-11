#ifndef CLIENT_H
#define CLIENT_H

#include "RingBuffer.hpp"
#include <string>

class Client {
public:
    Client(RingBuffer<std::string>& buffer);
    void run();

private:
    RingBuffer<std::string>& buffer_;
};

#endif // CLIENT_H
