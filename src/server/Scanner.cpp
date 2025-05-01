#include "Scanner.hpp"

Scanner::Scanner(PageTable* page_table, PolicyConfig* policy_config)
  : page_table_(page_table), policy_config_(policy_config) {
}

PageStatus Scanner::classifyPage(uint64_t last_access_time, uint32_t access_count) const
{
  if (classifyHotPage(last_access_time, access_count)) return PageStatus::HOT;
  if (!classifyColdPage(last_access_time, access_count)) return PageStatus::WARM;
  return PageStatus::COLD;
}

bool Scanner::classifyHotPage(const uint64_t last_access_time, uint32_t access_count) const
{
  auto current_time = boost::chrono::steady_clock::now();
  auto current_time_ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(
    current_time.time_since_epoch()).count();
  auto time_since_last_access = current_time_ms - last_access_time;

  if (policy_config_->policy_type == "lru") {
    auto& lru = std::get<LRUPolicyConfig>(policy_config_->config);
    return time_since_last_access <= lru.hot_threshold_ms;
  }
  else if (policy_config_->policy_type == "frequency") {
    auto& freq = std::get<FrequencyPolicyConfig>(policy_config_->config);
    return access_count >= freq.hot_access_count;
  }
  else if (policy_config_->policy_type == "hybrid") {
    auto& hybrid = std::get<HybridPolicyConfig>(policy_config_->config);
    double recency_score = (time_since_last_access <= hybrid.hot_threshold_ms) ? hybrid.weight_recency : 0.0;
    double frequency_score = (access_count >= hybrid.hot_access_count) ? hybrid.weight_frequency : 0.0;
    return (recency_score + frequency_score) >= (hybrid.weight_recency + hybrid.weight_frequency) / 2;
  }
  return false;
}

bool Scanner::classifyColdPage(const uint64_t last_access_time, uint32_t access_count) const
{
  auto current_time = boost::chrono::steady_clock::now();
  auto current_time_ms = boost::chrono::duration_cast<boost::chrono::milliseconds>(
    current_time.time_since_epoch()).count();
  auto time_since_last_access = current_time_ms - last_access_time;

  if (policy_config_->policy_type == "lru") {
    auto& lru = std::get<LRUPolicyConfig>(policy_config_->config);
    return time_since_last_access >= lru.cold_threshold_ms;
  }
  else if (policy_config_->policy_type == "frequency") {
    auto& freq = std::get<FrequencyPolicyConfig>(policy_config_->config);
    return access_count <= freq.cold_access_count;
  }
  else if (policy_config_->policy_type == "hybrid") {
    auto& hybrid = std::get<HybridPolicyConfig>(policy_config_->config);
    double recency_score = (time_since_last_access >= hybrid.cold_threshold_ms) ? hybrid.weight_recency : 0.0;
    double frequency_score = (access_count <= hybrid.cold_access_count) ? hybrid.weight_frequency : 0.0;
    return (recency_score + frequency_score) >= (hybrid.weight_recency + hybrid.weight_frequency) / 2;
  }
  return false;
}

void Scanner::runScanner(size_t num_tiers)
{
  auto scan_start_time = boost::chrono::steady_clock::now();
  while (!_shouldShutdown())
  {
    size_t page_id = page_table_->scanNext();
    PageLayer page_layer;
    uint64_t last_access_time;
    uint32_t access_cnt;
    std::tie(page_layer, last_access_time, access_cnt) = page_table_->getPageMetaData(page_id);

    PageStatus status = classifyPage(last_access_time, access_cnt);

    switch (page_layer)
    {
    case PageLayer::NUMA_LOCAL:
      if (status == PageStatus::COLD)
        page_table_->migratePage(page_id, num_tiers == 2 ? PageLayer::PMEM : PageLayer::NUMA_REMOTE);
      break;

    case PageLayer::NUMA_REMOTE:
      if (status == PageStatus::COLD)
        page_table_->migratePage(page_id, PageLayer::PMEM);
      else if (classifyHotPage(last_access_time, access_cnt))
        page_table_->migratePage(page_id, PageLayer::NUMA_LOCAL);
      break;

    case PageLayer::PMEM:
      if (status == PageStatus::HOT)
        page_table_->migratePage(page_id, PageLayer::NUMA_LOCAL);
      else if (status == PageStatus::WARM)
        page_table_->migratePage(page_id, num_tiers == 2 ? PageLayer::NUMA_LOCAL : PageLayer::NUMA_REMOTE);
      break;
    }

    if (page_id == page_table_->size() - 1)
    {

      auto scan_end_time = boost::chrono::steady_clock::now();
      auto scan_duration = boost::chrono::duration_cast<boost::chrono::seconds>(scan_end_time - scan_start_time).count();
      LOG_INFO("finished scanning all pages in one round at " << scan_duration << " seconds");

      page_table_->promoteToHugePage();
      auto promotion_end_time = boost::chrono::steady_clock::now();
      auto promotion_duration = boost::chrono::duration_cast<boost::chrono::seconds>(promotion_end_time - scan_end_time).count();
      LOG_INFO("finished promotion pages at " << promotion_duration << " seconds");
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
