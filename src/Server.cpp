#include "Server.hpp"
#include <iostream>

Server::Server(RingBuffer<std::string>& buffer) : buffer_(buffer) {}

void Server::run() {
    std::string msg;
    while (true) {  // Replace with condition for demonstration
        while (!buffer_.pop(msg)) {
            // Wait or yield if the buffer is empty
        }
        std::cout << "Server received: " << msg << std::endl;
    }
}
