#include "Client.hpp"
#include <iostream>

Client::Client(RingBuffer<std::string>& buffer) : buffer_(buffer) {}

void Client::run() {
    for (int i = 0; i < 100; ++i) {
        std::string msg = "Message " + std::to_string(i);
        while (!buffer_.push(msg)) {
            // Wait or yield if the buffer is full
        }
        std::cout << "Client sent: " << msg << std::endl;
    }
}
