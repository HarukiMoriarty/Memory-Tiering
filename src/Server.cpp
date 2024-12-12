#include "Server.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Server::Server(RingBuffer<Message>& client_buffer, const std::vector<int>& client_memory_sizes)
    : client_buffer_(client_buffer) {
    // Calculate base addresses for each client
    int current_base = 0;
    for (int size : client_memory_sizes) {
        base_addresses_.push_back(current_base);
        current_base += size;
    }
}

void Server::run() {
    std::string msg;
    while (true) {
        Message msg(0, 0, OperationType::READ);

        if (client_buffer_.pop(msg)) {
            std::cout << "Server received: " << msg.toString() << std::endl;
            int actual_address = base_addresses_[msg.client_id] + msg.offset;

            // Process the message based on operation type
            if (msg.op_type == OperationType::READ) {
                // Handle read operation
                std::cout << "Processing READ from address: " << actual_address << std::endl;
            }
            else {
                // Handle write operation
                std::cout << "Processing WRITE to address: " << actual_address << std::endl;
            }
        }
        else {
            boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        }
    }
}
