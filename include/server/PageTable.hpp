#ifndef PAGETABLE_H
#define PAGETABLE_H

#include <boost/thread/shared_mutex.hpp>
#include <boost/thread/locks.hpp>
#include <vector>

#include "Common.hpp"
#include "Utils.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"

struct PageMetadata {
    void* page_address;
    PageLayer page_layer;
    boost::chrono::steady_clock::time_point last_access_time;

    PageMetadata(void* addr = 0, PageLayer layer = PageLayer::NUMA_LOCAL) :
        page_address(addr),
        page_layer(layer),
        last_access_time(boost::chrono::steady_clock::now()) {
    }
};

class PageTable {
public:
    PageTable(size_t size, ServerMemoryConfig server_config);
    void initPageTable(const std::vector<ClientConfig>& client_configs, void* local_base, void* remote_base, void* pmem_base);

    // Read-only operations
    PageMetadata getPage(size_t index);
    size_t size() const { return table_.size(); };
    size_t scanNext();

    // Write operations  
    void accessPage(size_t index, mem_access_mode mode);
    void migratePage(size_t page_index, PageLayer new_layer);

private:
    std::vector<PageMetadata> table_;
    std::vector<boost::shared_mutex> mutexes_;
    size_t scan_index_ = 0;

    ServerMemoryConfig server_config_;
};

#endif // PAGETABLE_H