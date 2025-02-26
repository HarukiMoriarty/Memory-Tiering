#ifndef PAGETABLE_H
#define PAGETABLE_H

#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/unordered_map.hpp>
#include <vector>

#include "Common.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"
#include "Utils.hpp"

struct PageMetadata {
  void *page_address;
  PageLayer page_layer;
  boost::chrono::steady_clock::time_point last_access_time;

  PageMetadata(void *addr = 0, PageLayer layer = PageLayer::NUMA_LOCAL)
      : page_address(addr), page_layer(layer),
        last_access_time(boost::chrono::steady_clock::now()) {}
};

struct PageTableEntry {
  PageMetadata metadata;
  mutable boost::shared_mutex mutex;

  PageTableEntry(void *addr = 0, PageLayer layer = PageLayer::NUMA_LOCAL)
      : metadata(addr, layer) {}
};

class PageTable {
public:
  PageTable(const std::vector<ClientConfig> &client_configs,
            const ServerMemoryConfig server_config);
  ~PageTable();

  void initPageTable();

  // Read-only operations
  PageMetadata getPage(size_t page_id);
  size_t size() const { return table_.size(); };
  size_t scanNext();

  // Write operations
  void accessPage(size_t page_id, mem_access_mode mode);
  void migratePage(size_t page_id, PageLayer new_layer);

private:
  void _allocateMemory();
  void _generateRandomContent();

  std::vector<ClientConfig> client_configs_;
  ServerMemoryConfig server_config_;

  boost::unordered_map<size_t, PageTableEntry> table_;

  void *local_base_ = nullptr;
  void *remote_base_ = nullptr;
  void *pmem_base_ = nullptr;

  size_t local_page_load_ = 0;
  size_t remote_page_load_ = 0;
  size_t pmem_page_load_ = 0;

  size_t scan_index_ = 0;
};

#endif // PAGETABLE_H