#include "Server.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Server::Server(RingBuffer<ClientMessage>& client_buffer, RingBuffer<MemMoveReq>& move_page_buffer,
    const std::vector<ClientConfig>& client_configs, const ServerMemoryConfig& server_config, const PolicyConfig& policy_config)
    : client_buffer_(client_buffer), move_page_buffer_(move_page_buffer), server_config_(server_config), policy_config_(policy_config) {
    // Calculate load memory pages 
    if (server_config_.num_tiers == 2) {
        for (ClientConfig client : client_configs) {
            base_page_id_.push_back(local_page_load_ + pmem_page_load_);
            local_page_load_ += client.tier_sizes[0];
            pmem_page_load_ += client.tier_sizes[1];
        }
    }
    else if (server_config_.num_tiers == 3) {
        for (ClientConfig client : client_configs) {
            base_page_id_.push_back(local_page_load_ + remote_page_load_ + pmem_page_load_);
            local_page_load_ += client.tier_sizes[0];
            remote_page_load_ += client.tier_sizes[1];
            pmem_page_load_ += client.tier_sizes[2];
        }
    }
    server_config_.local_numa.count = local_page_load_;
    server_config_.remote_numa.count = remote_page_load_;
    server_config_.pmem.count = pmem_page_load_;
    size_t total_page_load = local_page_load_ + remote_page_load_ + pmem_page_load_;

    // Initialize flags for each client
    client_done_flags_ = std::vector<bool>(client_configs.size(), false);

    // Init PageTable with the total memory size
    page_table_ = new PageTable(total_page_load);
    scanner_ = new Scanner(*page_table_);

    // Allocate memory based on the server config
    allocateMemory();
    // After allocating memory, generate random content in all tiers
    generateRandomContent();

    // Init page table of all memory tiers
    page_table_->initPageTable(client_configs, server_config, local_base_, remote_base_, pmem_base_);
}

Server::~Server() {
    if (local_base_) {
        munmap(local_base_, local_page_load_ * PAGE_SIZE);
    }
    if (remote_base_) {
        munmap(remote_base_, remote_page_load_ * PAGE_SIZE);
    }
    if (pmem_base_) {
        munmap(pmem_base_, pmem_page_load_ * PAGE_SIZE);
    }
}

void Server::allocateMemory() {
    if (server_config_.num_tiers == 2) {
        // For 2 tiers, combine local and remote NUMA memory into DRAM
        // Allocate local NUMA memory
        local_base_ = allocate_pages(PAGE_SIZE, local_page_load_ + remote_page_load_);
    }
    else {
        // Allocate local NUMA memory
        local_base_ = allocate_and_bind_to_numa(PAGE_SIZE, local_page_load_, 0);
        // Allocate memory for remote NUMA pages 
        remote_base_ = allocate_and_bind_to_numa(PAGE_SIZE, remote_page_load_, 1);
    }

    // Allocate PMEM memory
    pmem_base_ = allocate_and_bind_to_numa(PAGE_SIZE, pmem_page_load_, 2);
}

void Server::generateRandomContent() {
    // Seed the random number generator
    srand(static_cast<unsigned>(time(NULL)));

    size_t local_size = (server_config_.num_tiers == 2) ? (local_page_load_ + remote_page_load_) * PAGE_SIZE : local_page_load_ * PAGE_SIZE;
    size_t remote_size = (server_config_.num_tiers == 3) ? remote_page_load_ * PAGE_SIZE : 0;
    size_t pmem_size = pmem_page_load_ * PAGE_SIZE;

    LOG_DEBUG("Local size: " << local_size);
    if (server_config_.num_tiers == 3) {
        LOG_DEBUG("Remote size: " << remote_size);
    }
    LOG_DEBUG("PMEM size: " << pmem_size);

    // Fill numa local tier
    {
        unsigned char* base = static_cast<unsigned char*>(local_base_);
        for (size_t i = 0; i < local_size; i++) {
            base[i] = static_cast<unsigned char>(rand() % 256);
        }
    }
    LOG_DEBUG("Random content generated for local numa node.");

    // Fill numa remote tier
    if (server_config_.num_tiers == 3) {
        unsigned char* base = static_cast<unsigned char*>(remote_base_);
        for (size_t i = 0; i < remote_size; i++) {
            base[i] = static_cast<unsigned char>(rand() % 256);
        }
        LOG_DEBUG("Random content generated for remote NUMA node.");
    }

    // Fill pmem tier
    {
        unsigned char* base = static_cast<unsigned char*>(pmem_base_);
        for (size_t i = 0; i < pmem_size; i++) {
            base[i] = static_cast<unsigned char>(rand() % 256);
        }
        LOG_DEBUG("Random content generated for persistent memory.");
    }
}

// Helper function to handle a ClientMessage
void Server::handleClientMessage(const ClientMessage& msg) {
    LOG_DEBUG("Server received: " << msg.toString());

    if (msg.op_type == OperationType::END) {
        client_done_flags_[msg.client_id] = true;
        LOG_DEBUG("Client " << msg.client_id << " sent END command.");

        // Check if all clients are done
        if (std::all_of(client_done_flags_.begin(), client_done_flags_.end(), [](bool done) { return done; })) {
            LOG_INFO("All clients sent END command. Printing metrics...");
            if (server_config_.num_tiers == 3) { Metrics::getInstance().printMetricsThreeTiers(); }
            else {
                Metrics::getInstance().printMetricsTwoTiers();
            }
            signalShutdown();  // Exit the server gracefully
        }
        return;
    }

    size_t page_id = base_page_id_[msg.client_id] + msg.pid;
    PageMetadata page_meta = page_table_->getPage(page_id);
    page_table_->updateAccess(page_id);

    switch (page_meta.page_layer) {
    case PageLayer::NUMA_LOCAL:
        Metrics::getInstance().incrementLocalAccess();
        break;
    case PageLayer::NUMA_REMOTE:
        Metrics::getInstance().incrementRemoteAccess();
        break;
    case PageLayer::PMEM:
        Metrics::getInstance().incrementPmemAccess();
        break;
    }

    // record access latency
    uint64_t access_time;
    if (msg.op_type == OperationType::READ) {
        access_time = access_page(page_meta.page_address, READ);
    }
    else {
        access_time = access_page(page_meta.page_address, WRITE);
    }
    Metrics::getInstance().recordAccessLatency(access_time);
    LOG_DEBUG("Access time: " << access_time << " ns");
}

// Helper function to handle a MemMoveReq
void Server::handleMemoryMoveRequest(const MemMoveReq& req) {
    LOG_DEBUG("Server received move request: " << req.toString());

    size_t page_id = static_cast<size_t>(req.page_id);
    PageMetadata page_meta = page_table_->getPage(page_id);
    PageLayer target_node = req.layer_id;
    PageLayer current_node = page_meta.page_layer;

    if (current_node == target_node) {
        LOG_WARN("Page " << page_id << " is already on the desired layer.");
        return;
    }

    // Get target layer info
    LayerInfo* target_info = nullptr;
    LayerInfo* current_info = nullptr;

    // Get layer info pointers
    switch (target_node) {
    case PageLayer::NUMA_LOCAL:  target_info = &server_config_.local_numa; break;
    case PageLayer::NUMA_REMOTE: target_info = &server_config_.remote_numa; break;
    case PageLayer::PMEM:        target_info = &server_config_.pmem; break;
    }
    switch (current_node) {
    case PageLayer::NUMA_LOCAL:  current_info = &server_config_.local_numa; break;
    case PageLayer::NUMA_REMOTE: current_info = &server_config_.remote_numa; break;
    case PageLayer::PMEM:        current_info = &server_config_.pmem; break;
    }

    // Check capacity
    if (target_info->isFull()) {
        LOG_DEBUG(target_node << " is full, page mitigate is failed");
        return;
    }

    // Update counters
    current_info->count--;
    target_info->count++;

    // Update metrics
    auto& metrics = Metrics::getInstance();
    if (current_node == PageLayer::NUMA_LOCAL) {
        if (target_node == PageLayer::NUMA_REMOTE) metrics.incrementLocalToRemote();
        else if (target_node == PageLayer::PMEM) metrics.incrementLocalToPmem();
    }
    else if (current_node == PageLayer::NUMA_REMOTE) {
        if (target_node == PageLayer::NUMA_LOCAL) metrics.incrementRemoteToLocal();
        else if (target_node == PageLayer::PMEM) metrics.incrementRemoteToPmem();
    }
    else if (current_node == PageLayer::PMEM) {
        if (target_node == PageLayer::NUMA_LOCAL) metrics.incrementPmemToLocal();
        else if (target_node == PageLayer::NUMA_REMOTE) metrics.incrementPmemToRemote();
    }

    // Perform the page migration
    LOG_DEBUG("Moving Page " << page_id << " from Node " << current_node << " to Node " << target_node << "...");
    uint64_t migrate_time = migrate_page(page_meta.page_address, current_node, target_node);
    metrics.recordMigrationLatency(migrate_time);
    LOG_DEBUG("Migration time: " << migrate_time << " ns");

    // Update the page layer in the PageTable
    page_table_->updatePageLayer(page_id, req.layer_id);
    LOG_DEBUG("Page " << page_id << " now on Layer " << req.layer_id);
}

void Server::runManagerThread() {
    while (!shouldShutdown()) {
        ClientMessage client_msg(0, 0, 0, OperationType::READ);
        MemMoveReq move_msg(0, PageLayer::NUMA_LOCAL);
        bool didwork = false;

        // Get memory request from client
        if (client_buffer_.pop(client_msg)) {
            LOG_DEBUG("Server received: " << client_msg.toString());
            handleClientMessage(client_msg);
            didwork = true;
        }

        // Get page move request from policy thread
        if (move_page_buffer_.pop(move_msg)) {
            LOG_DEBUG("Server received: " << move_msg.toString());
            handleMemoryMoveRequest(move_msg);
            didwork = true;
        }

        // Sleep if no works was done
        if (!didwork) {
            boost::this_thread::sleep_for(boost::chrono::nanoseconds(100));
        }
    }
    LOG_DEBUG("Manager thread exiting...");
}

// Policy thread logic
void Server::runPolicyThread() {
    scanner_->runClassifier(move_page_buffer_, policy_config_.hot_access_cnt, boost::chrono::milliseconds(policy_config_.cold_access_interval), server_config_.num_tiers);
    LOG_DEBUG("Policy thread exiting...");
}

void Server::signalShutdown() {
    boost::lock_guard<boost::mutex> lock(shutdown_mutex_);
    shutdown_flag_ = true;
    scanner_->stopClassifier();
}

bool Server::shouldShutdown() {
    boost::lock_guard<boost::mutex> lock(shutdown_mutex_);
    return shutdown_flag_;
}

// Main function to start threads
void Server::start() {
    boost::thread server_thread(&Server::runManagerThread, this);
    boost::thread policy_thread(&Server::runPolicyThread, this);

    // Join threads
    server_thread.join();
    policy_thread.join();

    LOG_INFO("All threads exited. Server shutdown complete.");
}
