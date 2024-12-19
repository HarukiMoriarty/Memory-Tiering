#include "Server.hpp"
#include "Common.hpp"
#include "Logger.hpp"

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

    // Allocate memory based on the server config
    allocateMemory(server_config);

    // Init page table of all memory tiers
    page_table_->initPageTable(client_addr_space, server_config, local_base_, remote_base_, pmem_base_);
}

Server::~Server() {
    if (local_base_) {
        munmap(local_base_, local_page_count_ * PAGE_SIZE);
    }
    if (remote_base_) {
        munmap(remote_base_, remote_page_count_ * PAGE_SIZE);
    }
    if (pmem_base_) {
        munmap(pmem_base_, pmem_page_count_ * PAGE_SIZE);
    }
}

void Server::allocateMemory(const ServerMemoryConfig& config) {
    // Store counts
    local_page_count_ = config.local_numa_size;
    remote_page_count_ = config.remote_numa_size;
    pmem_page_count_ = config.pmem_size;

    // Allocate local NUMA memory
    local_base_ = allocate_pages(PAGE_SIZE, local_page_count_);

    // Allocate memory for remote NUMA pages 
    remote_base_ = allocate_and_bind_to_numa(PAGE_SIZE, local_page_count_, 1);

    // Allocate PMEM pages
    pmem_base_ = allocate_and_bind_to_numa(PAGE_SIZE, local_page_count_, 2);
}

// Helper function to handle a ClientMessage
void Server::handleClientMessage(const ClientMessage& msg) {
    LOG_DEBUG("Server received: " << msg.toString());
    size_t actual_id = base_page_id_[msg.client_id] + msg.offset;

    // Update access info in PageTable (assume address maps directly to a page_id)
    size_t page_id = static_cast<size_t>(actual_id);
    void* actual_address = page_table_->getPage(actual_id).page_address;
    page_table_->updateAccess(page_id);

    // record access latency
    // TODO: notice we need actual transfer address
    uint64_t access_time;
    if (msg.op_type == OperationType::READ) {
        access_time = access_page(actual_address, READ);
    }
    else {
        access_time = access_page(actual_address, WRITE);
    }
    LOG_DEBUG("Access time: " << access_time << " ns");
}

// Helper function to handle a MemMoveReq
void Server::handleMemoryMoveRequest(const MemMoveReq& req) {
    LOG_DEBUG("Server received move request: " << req.toString());

    size_t page_id = static_cast<size_t>(req.page_id);
    PageMetadata page_meta = page_table_->getPage(page_id);

    // Determine the target NUMA node or memory layer
    PageLayer target_node = req.layer_id;
    PageLayer current_node = page_meta.page_layer;

    if (current_node == target_node) {
        LOG_DEBUG("Page " << page_id << " is already on the desired layer.");
        return;
    }

    // Perform the page migration
    LOG_DEBUG("Moving Page " << page_id
        << " from Node " << current_node
        << " to Node " << target_node << "...");
    migrate_page(page_meta.page_address, current_node, target_node);

    // After the move, update the page layer in the PageTable
    page_table_->updatePageLayer(page_id, req.layer_id);

    LOG_DEBUG("Page " << page_id << " now on Layer " << req.layer_id);
}

void Server::runManagerThread() {
    while (true) {
        ClientMessage client_msg(0, 0, OperationType::READ);
        MemMoveReq move_msg(0, PageLayer::NUMA_LOCAL);

        // Get memory request from client
        if (client_buffer_.pop(client_msg)) {
            LOG_DEBUG("Server received: " << client_msg.toString());
            handleClientMessage(client_msg);
        }

        // Get page move request from policy thread
        if (move_page_buffer_.pop(move_msg)) {
            LOG_DEBUG("Server received: " << move_msg.toString());
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
    // boost::thread policy_thread(&Server::runPolicyThread, this);

    // Join threads
    server_thread.join();
    // policy_thread.join();
}
