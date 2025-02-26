#include "Scanner.hpp"

Scanner::Scanner(PageTable &page_table) : page_table_(page_table) {}

// Check if a page is hot based on time threshold
bool Scanner::classifyHotPage(
    const PageMetadata &page,
    boost::chrono::milliseconds time_threshold) const {
  auto current_time = boost::chrono::steady_clock::now();
  auto time_since_last_access =
      boost::chrono::duration_cast<boost::chrono::milliseconds>(
          current_time - page.last_access_time);
  return time_since_last_access <= time_threshold;
}

// Check if a page is cold based on both access count and time threshold
bool Scanner::classifyColdPage(
    const PageMetadata &page,
    boost::chrono::milliseconds time_threshold) const {
  auto current_time = boost::chrono::steady_clock::now();
  auto time_since_last_access =
      boost::chrono::duration_cast<boost::chrono::milliseconds>(
          current_time - page.last_access_time);
  return time_since_last_access >= time_threshold;
}

// Continuously classify pages
void Scanner::runScanner(boost::chrono::milliseconds hot_time_threshold,
                         boost::chrono::milliseconds cold_time_threshold,
                         size_t num_tiers) {
  while (!_shouldShutdown()) {
    // We are safe to use shared lock to get meta data here,
    // even if the info is changed before classification.
    size_t page_id = page_table_.scanNext();
    PageMetadata page_meta_data = page_table_.getPage(page_id);

    switch (page_meta_data.page_layer) {
    case PageLayer::NUMA_LOCAL: {
      if (classifyColdPage(page_meta_data, cold_time_threshold)) {
        LOG_DEBUG("Cold page detected in DRAM: " << page_id);
        if (num_tiers == 2) {
          page_table_.migratePage(page_id, PageLayer::PMEM);
        } else {
          page_table_.migratePage(page_id, PageLayer::NUMA_REMOTE);
        }
      }
      break;
    }

    case PageLayer::NUMA_REMOTE: {
      // Check cold first, then hot if not cold
      if (classifyColdPage(page_meta_data, cold_time_threshold)) {
        LOG_DEBUG("Cold page detected in NUMA_REMOTE: " << page_id);
        page_table_.migratePage(page_id, PageLayer::PMEM);
      } else if (classifyHotPage(page_meta_data, hot_time_threshold)) {
        LOG_DEBUG("Hot page detected in NUMA_REMOTE: " << page_id);
        page_table_.migratePage(page_id, PageLayer::NUMA_LOCAL);
      }
      break;
    }

    case PageLayer::PMEM: {
      if (classifyHotPage(page_meta_data, hot_time_threshold)) {
        LOG_DEBUG("Hot page detected in PMEM: " << page_id);
        if (num_tiers == 2) {
          page_table_.migratePage(page_id, PageLayer::NUMA_LOCAL);
        } else {
          page_table_.migratePage(page_id, PageLayer::NUMA_REMOTE);
        }
      }
      break;
    }
    }

    // Sleep for a short duration after whole table iteration
    if (page_id == page_table_.size() - 1) {
      LOG_DEBUG("Finished scanning all pages in one round!");
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }
  }
}

bool Scanner::_shouldShutdown() {
  boost::lock_guard<boost::mutex> lock(scanner_shutdown_mutex_);
  return scanner_shutdown_flag_;
}

void Scanner::signalShutdown() {
  boost::lock_guard<boost::mutex> lock(scanner_shutdown_mutex_);
  scanner_shutdown_flag_ = true;
}