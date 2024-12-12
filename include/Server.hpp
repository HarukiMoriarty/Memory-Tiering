#ifndef SERVER_H
#define SERVER_H

#include <string>

#include "RingBuffer.hpp"
#include "Common.h"

class Server {
public:
    Server(RingBuffer<Message>& buffer);
    void run();

private:
    RingBuffer<Message>& buffer_;
};

#endif // SERVER_H
