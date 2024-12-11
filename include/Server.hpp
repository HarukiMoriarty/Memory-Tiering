#ifndef SERVER_H
#define SERVER_H

#include "RingBuffer.hpp"
#include <string>

class Server {
public:
    Server(RingBuffer<std::string>& buffer);
    void run();

private:
    RingBuffer<std::string>& buffer_;
};

#endif // SERVER_H
