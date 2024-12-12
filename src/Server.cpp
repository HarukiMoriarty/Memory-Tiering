#include "Server.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Server::Server(RingBuffer<Message>& buffer) : buffer_(buffer) {}

void Server::run() {
    std::string msg;
    while (true) {
        Message msg(nullptr, OperationType::READ);

        if (buffer_.pop(msg)) {
            std::cout << "Server received: " << msg.toString() << std::endl;

            // Process the message based on operation type
            if (msg.op_type == OperationType::READ) {
                // Handle read operation
                std::cout << "Processing READ from address: " << msg.page_address << std::endl;
            }
            else {
                // Handle write operation
                std::cout << "Processing WRITE to address: " << msg.page_address << std::endl;
            }
        }
        else {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        }
    }
}
