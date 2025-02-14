#include "Server.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"

#include <iostream>
#include <boost/chrono.hpp>
#include <boost/thread/thread.hpp> 

Server::Server(RingBuffer<ClientMessage>& client_buffer, const std::vector<ClientConfig>& client_configs,
    const ServerMemoryConfig& server_config, const PolicyConfig& policy_config)
    : client_buffer_(client_buffer), server_config_(server_config), policy_config_(policy_config) {
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
    page_table_ = new PageTable(total_page_load, server_config);
    scanner_ = new Scanner(*page_table_);

    // Allocate memory based on the server config
    allocateMemory();
    // After allocating memory, generate random content in all tiers
    generateRandomContent();

    // Init page table of all memory tiers
    page_table_->initPageTable(client_configs, local_base_, remote_base_, pmem_base_);
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
    if (msg.op_type == OperationType::END) {
        client_done_flags_[msg.client_id] = true;
        LOG_DEBUG("Client " << msg.client_id << " sent END command.");

        // Check if all clients are done
        if (std::all_of(client_done_flags_.begin(), client_done_flags_.end(), [](bool done) { return done; })) {
            LOG_INFO("All clients sent END command.");
            signalShutdown();  // Exit the server gracefully
        }
        return;
    }

    size_t page_index = base_page_id_[msg.client_id] + msg.pid;
    if (msg.op_type == OperationType::READ) {
        page_table_->accessPage(page_index, READ);
    }
    else {
        page_table_->accessPage(page_index, WRITE);
    }
}

void Server::runManagerThread() {
    while (!shouldShutdown()) {
        ClientMessage client_msg(0, 0, 0, OperationType::READ);
        bool didwork = false;

        // Get memory request from client
        if (client_buffer_.pop(client_msg)) {
            LOG_DEBUG("Server received: " << client_msg.toString());
            handleClientMessage(client_msg);
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
void Server::runScannerThread() {
    scanner_->runScanner(boost::chrono::milliseconds(policy_config_.hot_access_interval), boost::chrono::milliseconds(policy_config_.cold_access_interval), server_config_.num_tiers);
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
    boost::thread policy_thread(&Server::runScannerThread, this);

    // Join threads
    server_thread.join();
    policy_thread.join();

    LOG_INFO("All threads exited. Server shutdown complete.");
}
