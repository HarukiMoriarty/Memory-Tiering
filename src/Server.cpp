#include "Server.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Server::Server(RingBuffer<ClientMessage>& client_buffer, RingBuffer<MemMoveReq>& move_page_buffer, 
    const std::vector<int>& client_memory_sizes, const ServerMemoryConfig& server_config)
    : client_buffer_(client_buffer), move_page_buffer_(move_page_buffer), server_config_(server_config) {
    // Calculate base addresses for each client
    int current_base = 0;
    for (int size : client_memory_sizes) {
        base_addresses_.push_back(current_base);
        current_base += size;
    }
    
    // Init PageTable with the total memory size
    page_table_ = new PageTable(current_base);

    scanner_ = new Scanner(*page_table_);

    // Allocate memory based on the server config
    allocateMemory(server_config);

    // Init page table of all memory tiers
    page_table_->initPageTable(client_memory_sizes, server_config, local_base_, remote_base_, pmem_base_);
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

    // TODO: Allocate memory for remote NUMA pages 
    // (allocate locally first)
    remote_base_ = allocate_pages(PAGE_SIZE, remote_page_count_);
    // Move these pages to another NUMA node, e.g., node 1
    move_pages_to_node(remote_base_, PAGE_SIZE, remote_page_count_, 1);

    // TODO: Allocate PMEM pages
    int fd = open(PMEM_FILE, O_RDWR, 0666);
    if (fd < 0) {
        perror("open PMEM file failed");
        exit(EXIT_FAILURE);
    }
    pmem_base_ = allocate_pmem_pages(fd, PAGE_SIZE, pmem_page_count_);
    close(fd);
}

// Helper function to handle a ClientMessage
void Server::handleClientMessage(const ClientMessage& msg) {
    std::cout << "Server received: " << msg.toString() << std::endl;
    int actual_address = base_addresses_[msg.client_id] + msg.offset;

    // Update access info in PageTable (assume address maps directly to a page_id)
    size_t page_id = static_cast<size_t>(actual_address);
    page_table_->updateAccess(page_id);

    // record access latency
    uint64_t access_time;
    if (msg.op_type == OperationType::READ) {
        access_time = access_page(reinterpret_cast<void*>(actual_address), READ);
    } else {
        access_time = access_page(reinterpret_cast<void*>(actual_address), WRITE);
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
