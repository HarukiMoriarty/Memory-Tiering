#include "Client.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Client::Client(RingBuffer<std::string>& buffer) : buffer_(buffer) {}

void Client::run() {
    for (int i = 0; i < 100; ++i) {
        std::string msg = "Message " + std::to_string(i);
        while (!buffer_.push(msg)) {
            boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
        }
        std::cout << "Client sent: " << msg << std::endl;
    }
}
