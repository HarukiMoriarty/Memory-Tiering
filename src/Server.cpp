#include "Server.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Server::Server(RingBuffer<Message>& client_buffer, RingBuffer<MemMoveReq>& move_page_buffer, const std::vector<int>& client_memory_sizes)
    : client_buffer_(client_buffer), move_page_buffer_(move_page_buffer) {
    // Calculate base addresses for each client
    int current_base = 0;
    for (int size : client_memory_sizes) {
        base_addresses_.push_back(current_base);
        current_base += size;
    }
}

void Server::runManagerThread() {
    while (true) {
        Message msg(0, 0, OperationType::READ);
        MemMoveReq move_msg(0, 0);

        // Get memory request from client
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
            // boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        }

        // Get page move request from policy thread
        if (move_page_buffer_.pop(move_msg)) {
            std::cout << "Server received: " << move_msg.toString() << std::endl;
            // Add your page move logic here
        }
        else {
            // boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        }
    }
}

// Policy thread logic
void Server::runPolicyThread() {
    // TODO: Scan

    // TODO: Policy -> move page decision
}

// Main function to start threads
void Server::start() {
    boost::thread server_thread(&Server::runManagerThread, this);
    boost::thread policy_thread(&Server::runPolicyThread, this);

    // Join threads
    server_thread.join();
    policy_thread.join();
}
