#ifndef PAGETABLE_H
#define PAGETABLE_H

#include <atomic>
#include <boost/thread/locks.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/unordered_map.hpp>
#include <tuple>
#include <vector>

#include "Common.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"
#include "Utils.hpp"

struct PageMetadata {
  std::atomic<PageLayer> page_layer;
  std::atomic<uint64_t> last_access_time_ms;

  PageMetadata(PageLayer layer = PageLayer::NUMA_LOCAL) : page_layer(layer) {
    auto now = boost::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    auto ms =
        boost::chrono::duration_cast<boost::chrono::microseconds>(duration)
            .count();
    last_access_time_ms.store(ms);
  }
};

struct PageTableEntry {
  void *page_address;
  PageMetadata metadata;

  // NOTICE: we only "protect" page address.
  // though page move syscall will not change VPN,
  // we prevent page being accessed during migration.
  mutable boost::shared_mutex mutex;

  PageTableEntry(void *addr = 0, PageLayer layer = PageLayer::NUMA_LOCAL)
      : page_address(addr), metadata(layer) {}
};

class PageTable {
public:
  PageTable(const std::vector<ClientConfig> &client_configs,
            ServerMemoryConfig *server_config);
  ~PageTable();

  void initPageTable();

  // Read-only operations
  std::tuple<PageLayer, uint64_t> getPageMetaData(size_t page_id);
  size_t size() const { return table_.size(); };
  size_t scanNext();

  // Write operations
  void accessPage(size_t page_id, OperationType mode);
  void migratePage(size_t page_id, PageLayer new_layer);

private:
  void _allocateMemory();
  void _generateRandomContent();

  std::vector<ClientConfig> client_configs_;
  ServerMemoryConfig *server_config_;

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