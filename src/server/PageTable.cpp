#include "PageTable.hpp"
#include "Logger.hpp"

PageMetadata::PageMetadata(void* addr, PageLayer layer)
    : page_address(addr),
    page_layer(layer),
    last_access_time(boost::chrono::steady_clock::now()),
    access_count(0)
{
}

PageTable::PageTable(size_t size) : table_(size) {}

void PageTable::initPageTable(const std::vector<size_t>& client_addr_space, const ServerMemoryConfig& server_config,
    void* local_base, void* remote_base, void* pmem_base) {
    // Calculate total number of pages required by all clients
    size_t total_pages = 0;
    for (auto size : client_addr_space) {
        total_pages += size;
    }

    LOG_DEBUG("Total pages required by clients: " << total_pages);
    // Lock the table for write
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    // Clear and resize the table to hold all required pages
    table_.clear();
    table_.resize(total_pages);

    // Extract server capacities
    size_t local_capacity = server_config.local_numa_size;
    size_t remote_capacity = server_config.remote_numa_size;
    size_t pmem_capacity = server_config.pmem_size;
    size_t num_tiers = server_config.num_tiers;
    
    LOG_DEBUG("Server capacities - Local NUMA: " << local_capacity << ", Remote NUMA: " << remote_capacity 
            << ", PMEM: " << pmem_capacity << ", Tiers: " << num_tiers);

    // Compute per-client allocation for NUMA_LOCAL
    std::vector<size_t> local_allocation(client_addr_space.size(), 0);
    std::vector<size_t> remote_allocation(client_addr_space.size(), 0);
    std::vector<size_t> pmem_allocation(client_addr_space.size(), 0);

    for (size_t i = 0; i < client_addr_space.size(); ++i) {
        if (num_tiers == 2) {
            // Proportional allocation for DRAM (local + remote in 2-tier setup)
            double dram_fraction = (double)client_addr_space[i] * (double)(local_capacity + remote_capacity) / (double)total_pages;
            local_allocation[i] = static_cast<size_t>(std::min(std::floor(dram_fraction), (double)client_addr_space[i]));
        } else {
            // Proportional allocation for local NUMA in 3-tier setup
            double local_fraction = (double)client_addr_space[i] * (double)local_capacity / (double)total_pages;
            local_allocation[i] = static_cast<size_t>(std::min(std::floor(local_fraction), (double)client_addr_space[i]));
        }
        LOG_DEBUG("Client " << i << " local allocation: " << local_allocation[i]);
    }

    // Compute per-client allocation for NUMA_REMOTE in 3-tier setup
    if (num_tiers == 3) {
        for (size_t i = 0; i < client_addr_space.size(); ++i) {
            size_t need_after_local = client_addr_space[i] - local_allocation[i];
            if (need_after_local > 0) {
                double remote_fraction = (double)client_addr_space[i] * (double)remote_capacity / (double)total_pages;
                size_t ideal_remote = static_cast<size_t>(std::floor(remote_fraction));
                // Assign the minimum of what they ideally get and what they still need
                remote_allocation[i] = std::min(ideal_remote, need_after_local);
            }
            else {
                remote_allocation[i] = 0;
            }
        }
    }

    // Compute per-client allocation for PMEM
    for (size_t i = 0; i < client_addr_space.size(); ++i) {
        size_t allocated_so_far = local_allocation[i] + remote_allocation[i];
        size_t need_after_remote = client_addr_space[i] - allocated_so_far;
        LOG_DEBUG("  - Total Address Space: " << client_addr_space[i]);
        LOG_DEBUG("  - Allocated So Far: " << allocated_so_far);
        LOG_DEBUG("  - Remaining Need After Remote: " << need_after_remote);
        if (need_after_remote > 0) {
            // Assign what's left to PMEM
            pmem_allocation[i] = need_after_remote;
        }
        else {
            pmem_allocation[i] = 0;
        }
        LOG_DEBUG("Client " << i << " PMEM allocation: " << pmem_allocation[i]);
    }

    // Check if we've allocated all pages correctly
    // Sum all allocated to verify correctness if needed
    size_t allocated_check = 0;
    for (size_t i = 0; i < client_addr_space.size(); ++i) {
        allocated_check += (local_allocation[i] + remote_allocation[i] + pmem_allocation[i]);
    }

    if (allocated_check != total_pages) {
        // This means something went off in allocation logic
        // For now, just assert or throw an exception.
        throw std::runtime_error("Total allocation mismatch!");
    }

    // Now fill the page table with metadata
    // We can fill in order: first all allocations for client 1 (local, remote, pmem),
    // then client 2, etc.

    size_t current_index = 0;
    size_t local_offset_pages = 0;
    size_t remote_offset_pages = 0;
    size_t pmem_offset_pages = 0;

    auto fillPages = [&](PageLayer layer, size_t count, void* base, size_t& offset) {
        for (size_t i = 0; i < count; ++i) {
            char* addr = static_cast<char*>(base) + offset * PAGE_SIZE;
            table_[current_index] = PageMetadata(static_cast<void*>(addr), layer);
            current_index++;
            offset++;
        }
        LOG_DEBUG("Filled " << count << " pages for layer " << static_cast<int>(layer));
        };

    for (size_t client_id = 0; client_id < client_addr_space.size(); ++client_id) {
        fillPages(PageLayer::NUMA_LOCAL, local_allocation[client_id], local_base, local_offset_pages);
        fillPages(PageLayer::NUMA_REMOTE, remote_allocation[client_id], remote_base, remote_offset_pages);
        fillPages(PageLayer::PMEM, pmem_allocation[client_id], pmem_base, pmem_offset_pages);
    }
}

PageMetadata PageTable::getPage(size_t index) const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        return table_[index];
    }
    return PageMetadata();
}

size_t PageTable::getNextPageId() const {
    return current_index_;
}

size_t PageTable::size() const {
    boost::shared_lock<boost::shared_mutex> lock(mutex_);
    return table_.size();
}

void PageTable::updatePage(size_t index, const PageMetadata& metadata) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        table_[index] = metadata;
    }
}

void PageTable::updateAccess(size_t index) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        table_[index].last_access_time = boost::chrono::steady_clock::now();
        table_[index].access_count++;
    }
}

void PageTable::updatePageLayer(size_t index, PageLayer new_layer) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        table_[index].page_layer = new_layer;
        table_[index].last_access_time = boost::chrono::steady_clock::now();
        table_[index].access_count++;
    }
}

PageMetadata PageTable::scanNext() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

    if (table_.empty()) {
        return PageMetadata(); // Handle empty table case
    }

    // Return the current page and move to the next index (circularly)
    PageMetadata page = table_[current_index_];
    current_index_ = (current_index_ + 1) % table_.size(); // Wrap around using modulo

    return page;
}

void PageTable::resetAccessCount() {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    for (auto& page : table_) {
        page.access_count = 0; // Reset access count
    }
}