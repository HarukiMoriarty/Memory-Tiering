#include "PageTable.hpp"

PageTable::PageTable(size_t size, ServerMemoryConfig server_config) :
    table_(size), server_config_(server_config) {
}

void PageTable::initPageTable(const std::vector<ClientConfig>& client_configs, void* local_base, void* remote_base, void* pmem_base) {
    // Fill in order: first all allocations for client 1 (local, remote, pmem), then client 2, etc.

    size_t current_index = 0;
    size_t local_offset_pages = 0;
    size_t remote_offset_pages = 0;
    size_t pmem_offset_pages = 0;

    auto fillPages = [&](PageLayer layer, size_t count, void* base, size_t& offset) {
        for (size_t i = 0; i < count; ++i) {
            char* addr = static_cast<char*>(base) + offset * PAGE_SIZE;

            table_.emplace(
                std::piecewise_construct,
                std::forward_as_tuple(current_index),
                std::forward_as_tuple(static_cast<void*>(addr), layer)
            );

            current_index++;
            offset++;
        }
        LOG_DEBUG("Filled " << count << " pages for layer " << static_cast<int>(layer));
        };

    for (ClientConfig client : client_configs) {
        if (server_config_.num_tiers == 2) {
            fillPages(PageLayer::NUMA_LOCAL, client.tier_sizes[0], local_base, local_offset_pages);
            server_config_.local_numa.count += client.tier_sizes[0];
            fillPages(PageLayer::PMEM, client.tier_sizes[1], pmem_base, pmem_offset_pages);
            server_config_.pmem.count += client.tier_sizes[1];
        }
        else if (server_config_.num_tiers == 3) {
            fillPages(PageLayer::NUMA_LOCAL, client.tier_sizes[0], local_base, local_offset_pages);
            server_config_.local_numa.count += client.tier_sizes[0];
            fillPages(PageLayer::NUMA_REMOTE, client.tier_sizes[1], remote_base, remote_offset_pages);
            server_config_.remote_numa.count += client.tier_sizes[1];
            fillPages(PageLayer::PMEM, client.tier_sizes[2], pmem_base, pmem_offset_pages);
            server_config_.pmem.count += client.tier_sizes[2];
        }
    }
}

PageMetadata PageTable::getPage(size_t page_id) {
    auto it = table_.find(page_id);
    if (it == table_.end()) {
        LOG_ERROR("Get Page metadata index " << page_id << " not found");
        return PageMetadata();
    }
    // This has an possible race condition:
    // After scanning the metadata, this page is accessed. This might caused
    // False cold page.
    boost::shared_lock<boost::shared_mutex> lock(it->second.mutex);
    return it->second.metadata;
}

size_t PageTable::scanNext() {
    size_t current = scan_index_;
    scan_index_++;
    if (scan_index_ >= table_.size()) {
        scan_index_ = 0;
    }
    return current;
}

void PageTable::accessPage(size_t page_id, mem_access_mode mode) {
    auto it = table_.find(page_id);
    if (it == table_.end()) {
        LOG_ERROR("Update Page access index " << page_id << " not found");
        return;
    }

    boost::unique_lock<boost::shared_mutex> unique_lock(it->second.mutex);
    PageMetadata& page_meta_data = it->second.metadata;
    uint64_t access_time = access_page(page_meta_data.page_address, mode);
    page_meta_data.last_access_time = boost::chrono::steady_clock::now();
    unique_lock.unlock();

    Metrics::getInstance().recordAccessLatency(access_time);
    switch (page_meta_data.page_layer) {
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
    LOG_DEBUG("Access " << page_id << " time: " << access_time << " ns");
}

void PageTable::migratePage(size_t page_index, PageLayer page_target_layer) {
    auto it = table_.find(page_index);
    if (it == table_.end()) {
        LOG_ERROR("Update Page layer index " << page_index << " not found");
        return;
    }

    boost::unique_lock<boost::shared_mutex> unique_lock(it->second.mutex);
    PageMetadata& page_meta_data = it->second.metadata;
    PageLayer page_current_layer = page_meta_data.page_layer;

    // Get target layer info
    LayerInfo* target_layer_info = nullptr;
    LayerInfo* current_layer_info = nullptr;

    // Get layer info pointers
    switch (page_target_layer) {
    case PageLayer::NUMA_LOCAL:  target_layer_info = &server_config_.local_numa; break;
    case PageLayer::NUMA_REMOTE: target_layer_info = &server_config_.remote_numa; break;
    case PageLayer::PMEM:        target_layer_info = &server_config_.pmem; break;
    }
    switch (page_current_layer) {
    case PageLayer::NUMA_LOCAL:  current_layer_info = &server_config_.local_numa; break;
    case PageLayer::NUMA_REMOTE: current_layer_info = &server_config_.remote_numa; break;
    case PageLayer::PMEM:        current_layer_info = &server_config_.pmem; break;
    }

    // Check capacity
    if (target_layer_info->isFull()) {
        LOG_DEBUG(page_target_layer << " is full, page mitigate is failed");
        return;
    }

    // Perform the page migration
    LOG_DEBUG("Moving Page " << page_index << " from Node " << page_current_layer << " to Node " << page_target_layer << "...");
    migrate_page(page_meta_data.page_address, page_current_layer, page_target_layer);
    // Maintain metadata
    page_meta_data.page_layer = page_target_layer;
    page_meta_data.last_access_time = boost::chrono::steady_clock::now();
    LOG_DEBUG("Page " << page_index << " now on Layer " << page_target_layer);

    unique_lock.unlock();

    // Update counters, for now scanner run in single thread, we do not need 
    // Protect layer info count.
    current_layer_info->count--;
    target_layer_info->count++;

    // Update metrics
    auto& metrics = Metrics::getInstance();
    if (page_current_layer == PageLayer::NUMA_LOCAL) {
        if (page_target_layer == PageLayer::NUMA_REMOTE) metrics.incrementLocalToRemote();
        else if (page_target_layer == PageLayer::PMEM) metrics.incrementLocalToPmem();
    }
    else if (page_current_layer == PageLayer::NUMA_REMOTE) {
        if (page_target_layer == PageLayer::NUMA_LOCAL) metrics.incrementRemoteToLocal();
        else if (page_target_layer == PageLayer::PMEM) metrics.incrementRemoteToPmem();
    }
    else if (page_current_layer == PageLayer::PMEM) {
        if (page_target_layer == PageLayer::NUMA_LOCAL) metrics.incrementPmemToLocal();
        else if (page_target_layer == PageLayer::NUMA_REMOTE) metrics.incrementPmemToRemote();
    }
}