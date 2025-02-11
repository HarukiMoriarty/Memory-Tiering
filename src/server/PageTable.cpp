#include "PageTable.hpp"
#include "Logger.hpp"

PageMetadata::PageMetadata(void* addr, PageLayer layer)
    : page_address(addr),
    page_layer(layer),
    last_access_time(boost::chrono::steady_clock::now())
{
}

PageTable::PageTable(size_t size) : table_(size) {}

void PageTable::initPageTable(const std::vector<ClientConfig>& client_configs, const ServerMemoryConfig& server_config,
    void* local_base, void* remote_base, void* pmem_base) {
    // Lock the table for write
    boost::unique_lock<boost::shared_mutex> lock(mutex_);

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

    for (ClientConfig client : client_configs) {
        if (server_config.num_tiers == 2) {
            fillPages(PageLayer::NUMA_LOCAL, client.tier_sizes[0], local_base, local_offset_pages);
            fillPages(PageLayer::PMEM, client.tier_sizes[1], pmem_base, pmem_offset_pages);
        }
        else if (server_config.num_tiers == 3) {
            fillPages(PageLayer::NUMA_LOCAL, client.tier_sizes[0], local_base, local_offset_pages);
            fillPages(PageLayer::NUMA_REMOTE, client.tier_sizes[1], remote_base, remote_offset_pages);
            fillPages(PageLayer::PMEM, client.tier_sizes[2], pmem_base, pmem_offset_pages);
        }
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
    }
}

void PageTable::updatePageLayer(size_t index, PageLayer new_layer) {
    boost::unique_lock<boost::shared_mutex> lock(mutex_);
    if (index < table_.size()) {
        table_[index].page_layer = new_layer;
        table_[index].last_access_time = boost::chrono::steady_clock::now();
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