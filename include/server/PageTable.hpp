#ifndef PAGETABLE_H
#define PAGETABLE_H

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <vector>

#include "Common.hpp"
#include "Utils.hpp"

struct PageMetadata {
    void* page_address;
    PageLayer page_layer;
    boost::chrono::steady_clock::time_point last_access_time;
    size_t access_count = 0;

    PageMetadata(void* addr = 0, PageLayer layer = PageLayer::NUMA_LOCAL);
};

class PageTable {
public:
    PageTable(size_t size);
    void initPageTable(const std::vector<ClientConfig>& client_configs, const ServerMemoryConfig& server_config, void* local_base, void* remote_base, void* pmem_base);

    // Read-only operations
    PageMetadata getPage(size_t index) const;
    size_t getNextPageId() const;
    size_t size() const;

    // Write operations  
    void updatePage(size_t index, const PageMetadata& metadata);
    void updateAccess(size_t index);
    void updatePageLayer(size_t index, PageLayer new_layer);
    PageMetadata scanNext();
    void resetAccessCount();

private:
    std::vector<PageMetadata> table_;
    mutable boost::shared_mutex mutex_;
    size_t current_index_;
};

#endif // PAGETABLE_H