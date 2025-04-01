#include "Scanner.hpp"

Scanner::Scanner(PageTable *page_table, PolicyConfig *policy_config)
    : page_table_(page_table), policy_config_(policy_config) {}

// Check if a page is hot based on time threshold
bool Scanner::classifyHotPage(const uint64_t last_access_time) const
{
  auto current_time = boost::chrono::steady_clock::now();
  auto current_time_ms =
      boost::chrono::duration_cast<boost::chrono::milliseconds>(
          current_time.time_since_epoch())
          .count();
  auto time_since_last_access = current_time_ms - last_access_time;
  return time_since_last_access <= policy_config_->hot_access_interval;
}

// Check if a page is cold based on both access count and time threshold
bool Scanner::classifyColdPage(const uint64_t last_access_time) const
{
  auto current_time = boost::chrono::steady_clock::now();
  auto current_time_ms =
      boost::chrono::duration_cast<boost::chrono::milliseconds>(
          current_time.time_since_epoch())
          .count();
  auto time_since_last_access = current_time_ms - last_access_time;
  return time_since_last_access >= policy_config_->cold_access_interval;
}

// Continuously classify pages
void Scanner::runScanner(size_t num_tiers)
{
  auto scan_start_time = boost::chrono::steady_clock::now();
  while (!_shouldShutdown())
  {
    // We are safe to use shared lock to get meta data here,
    // even if the info is changed before classification.
    size_t page_id = page_table_->scanNext();
    PageLayer page_layer;
    uint64_t last_access_time;
    std::tie(page_layer, last_access_time) =
        page_table_->getPageMetaData(page_id);

    switch (page_layer)
    {
    case PageLayer::NUMA_LOCAL:
    {
      if (classifyColdPage(last_access_time))
      {
        LOG_DEBUG("Cold page detected in DRAM: " << page_id);
        if (num_tiers == 2)
        {
          page_table_->migratePage(page_id, PageLayer::PMEM);
        }
        else
        {
          page_table_->migratePage(page_id, PageLayer::NUMA_REMOTE);
        }
      }
      break;
    }

    case PageLayer::NUMA_REMOTE:
    {
      // Check cold first, then hot if not cold
      if (classifyColdPage(last_access_time))
      {
        LOG_DEBUG("Cold page detected in NUMA_REMOTE: " << page_id);
        page_table_->migratePage(page_id, PageLayer::PMEM);
      }
      else if (classifyHotPage(last_access_time))
      {
        LOG_DEBUG("Hot page detected in NUMA_REMOTE: " << page_id);
        page_table_->migratePage(page_id, PageLayer::NUMA_LOCAL);
      }
      break;
    }

    case PageLayer::PMEM:
    {
      if (classifyHotPage(last_access_time))
      {
        LOG_DEBUG("Hot page detected in PMEM: " << page_id);
        if (num_tiers == 2)
        {
          page_table_->migratePage(page_id, PageLayer::NUMA_LOCAL);
        }
        else
        {
          page_table_->migratePage(page_id, PageLayer::NUMA_REMOTE);
        }
      }
      break;
    }
    }

    // Sleep for a short duration after whole table iteration
    if (page_id == page_table_->size() - 1)
    {
      // Promotion page to large page
      page_table_->promoteToHugePage();

      // Record scanning time metrics
      auto scan_end_time = boost::chrono::steady_clock::now();
      auto scan_duration = boost::chrono::duration_cast<boost::chrono::seconds>(
                               scan_end_time - scan_start_time)
                               .count();
      LOG_INFO("finished scanning all pages in one round at " << scan_duration
                                                              << " seconds");

      // Sleep before starting next round
      boost::this_thread::sleep_for(
          boost::chrono::seconds(policy_config_->scan_interval));
    }
  }
}

bool Scanner::_shouldShutdown()
{
  boost::lock_guard<boost::mutex> lock(scanner_shutdown_mutex_);
  return scanner_shutdown_flag_;
}

void Scanner::signalShutdown()
{
  boost::lock_guard<boost::mutex> lock(scanner_shutdown_mutex_);
  scanner_shutdown_flag_ = true;
}