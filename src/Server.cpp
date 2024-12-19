#include "Server.hpp"
#include "Common.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Server::Server(RingBuffer<ClientMessage>& client_buffer, RingBuffer<MemMoveReq>& move_page_buffer,
    const std::vector<size_t>& client_addr_space, const ServerMemoryConfig& server_config)
    : client_buffer_(client_buffer), move_page_buffer_(move_page_buffer), server_config_(server_config) {
    // Calculate base addresses for each client
    size_t current_base = 0;
    for (size_t size : client_addr_space) {
        base_page_id_.push_back(current_base);
        current_base += size;
    }

    // Init PageTable with the total memory size
    page_table_ = new PageTable(current_base);
    scanner_ = new Scanner(*page_table_);
}

// Helper function to handle a ClientMessage
void Server::handleClientMessage(const ClientMessage& msg) {
    std::cout << "Server received: " << msg.toString() << std::endl;
    size_t actual_id = base_page_id_[msg.client_id] + msg.offset;

    // Update access info in PageTable (assume address maps directly to a page_id)
    size_t page_id = static_cast<size_t>(actual_id);
    page_table_->updateAccess(page_id);

    // record access latency
    // TODO: notice we need actual transfer address
    uint64_t access_time;
    if (msg.op_type == OperationType::READ) {
        access_time = access_page(reinterpret_cast<void*>(actual_id), READ);
    }
    else {
        access_time = access_page(reinterpret_cast<void*>(actual_id), WRITE);
    }
    std::cout << "Access time: " << access_time << " ns" << std::endl;
}

// Helper function to handle a MemMoveReq
void Server::handleMemoryMoveRequest(const MemMoveReq& req) {
    std::cout << "Server received move request: " << req.toString() << std::endl;

    size_t page_id = static_cast<size_t>(req.page_id);
    PageMetadata page_meta = page_table_->getPage(page_id);

    if (page_meta.page_id < 0) {
        std::cerr << "Invalid page_id: " << req.page_id << std::endl;
        return;
    }

    // Determine the target NUMA node or memory layer
    PageLayer target_node = req.layer_id;
    PageLayer current_node = page_meta.page_layer;

    if (current_node == target_node) {
        std::cout << "Page " << page_id << " is already on the desired layer." << std::endl;
        return;
    }

    // Perform the page migration
    std::cout << "Moving Page " << page_id
        << " from Node " << current_node
        << " to Node " << target_node << "..." << std::endl;
    migrate_page(page_meta.page_address, current_node, target_node);

    // After the move, update the page layer in the PageTable
    page_table_->updatePageLayer(page_id, req.layer_id);

    std::cout << "Page " << page_id << " now on Layer " << req.layer_id << std::endl;
}

void Server::runManagerThread() {
    while (true) {
        ClientMessage client_msg(0, 0, OperationType::READ);
        MemMoveReq move_msg(0, PageLayer::NUMA_LOCAL);

        // Get memory request from client
        if (client_buffer_.pop(client_msg)) {
            std::cout << "Server received: " << client_msg.toString() << std::endl;
            handleClientMessage(client_msg);
        }

        // Get page move request from policy thread
        if (move_page_buffer_.pop(move_msg)) {
            std::cout << "Server received: " << move_msg.toString() << std::endl;
            handleMemoryMoveRequest(move_msg);
        }
        else {
            // boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
        }
    }
}

// Policy thread logic
void Server::runPolicyThread() {
    // TODO: Scan
    // Pre-defined thresholds
    size_t min_access_count = 10;
    std::chrono::seconds time_threshold(10);
    scanner_->runClassifier(move_page_buffer_, min_access_count, time_threshold);
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
