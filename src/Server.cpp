#include "Server.hpp"
#include <iostream>

Server::Server(RingBuffer<std::string>& buffer) : buffer_(buffer) {}

void Server::run() {
    std::string msg;
    while (true) {
        while (!buffer_.pop(msg)) {
            // TODO: Wait or yield if the buffer is empty
        }
        std::cout << "Server received: " << msg << std::endl;
    }
}
