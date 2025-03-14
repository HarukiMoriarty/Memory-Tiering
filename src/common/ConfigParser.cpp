#include "ConfigParser.hpp"
#include "Logger.hpp"

std::vector<std::string> ConfigParser::_split(const std::string &s,
                                              char delim) {
  std::vector<std::string> result;
  std::stringstream ss(s);
  std::string item;
  while (getline(ss, item, delim)) {
    result.push_back(item);
  }
  return result;
}

/**
 * Constructor: initializes command line options with default values
 */
ConfigParser::ConfigParser()
    : options_("MemoryTiering", "Concurrent Ring Buffer Demonstration"),
      help_requested_(false) {

  options_.add_options()("b,buffer-size", "Size of ring buffer",
                         cxxopts::value<size_t>()->default_value("10"))(
      "c,client-tier-sizes",
      "Memory space size per tier per client "
      "(client1_local,client1_remote,client1_pmem ...)",
      cxxopts::value<std::vector<std::string>>())(
      "cold-access-interval", "Cold page interval (ms)",
      cxxopts::value<size_t>()->default_value("1000"))(
      "hot-access-interval", "Hot page interval (ms)",
      cxxopts::value<size_t>()->default_value("100"))(
      "running-time", "Running time in seconds per client",
      cxxopts::value<size_t>())(
      "o,output", "Output file for access latency CDF data",
      cxxopts::value<std::string>()->default_value("result/latency.csv"))(
      "p,patterns",
      "Memory access patterns for each client (uniform/hot/zipfian)",
      cxxopts::value<std::vector<std::string>>())(
      "r,ratio", "Memory access read/write ratio",
      cxxopts::value<double>()->default_value("1.0"))(
      "s,mem-sizes", "Memory size in pages for each tier",
      cxxopts::value<std::vector<size_t>>())(
      "sample-rate", "Periodical sampleing rate",
      cxxopts::value<size_t>()->default_value("10"))(
      "scan-interval", "scan interval (s)",
      cxxopts::value<size_t>()->default_value("30"))(
      "t,num-tiers", "Number of memory tiers",
      cxxopts::value<size_t>()->default_value("3"))("h,help",
                                                    "Print usage information");
}

bool ConfigParser::_parseBasicConfig(const cxxopts::ParseResult &result) {
  buffer_size_ = result["buffer-size"].as<size_t>();
  running_time_ = result["running-time"].as<size_t>();
  policy_config_.hot_access_interval =
      result["hot-access-interval"].as<size_t>();
  policy_config_.cold_access_interval =
      result["cold-access-interval"].as<size_t>();
  policy_config_.scan_interval = result["scan-interval"].as<size_t>();
  server_memory_config_.num_tiers = result["num-tiers"].as<size_t>();
  cdf_output_file_ = result["output"].as<std::string>();
  rw_ratio_ = result["ratio"].as<double>();
  sample_rate_ = result["sample-rate"].as<size_t>();

  if (server_memory_config_.num_tiers < 2 ||
      server_memory_config_.num_tiers > 3) {
    LOG_ERROR("Number of tiers must be between 2 and 3");
    return false;
  }
  return true;
}

bool ConfigParser::_parseServerConfig(const cxxopts::ParseResult &result) {
  auto mem_sizes = result["mem-sizes"].as<std::vector<size_t>>();
  if (mem_sizes.size() != server_memory_config_.num_tiers) {
    LOG_ERROR("Server configuration must have exactly "
              << server_memory_config_.num_tiers << " memory sizes");
    return false;
  }

  server_memory_config_.local_numa.capacity = mem_sizes[0];
  server_memory_config_.remote_numa.capacity =
      (server_memory_config_.num_tiers == 3) ? mem_sizes[1] : 0;
  server_memory_config_.pmem.capacity = mem_sizes.back();
  return true;
}

bool ConfigParser::_parseClientConfigs(const cxxopts::ParseResult &result) {
  std::vector<std::string> patterns =
      result["patterns"].as<std::vector<std::string>>();
  std::vector<std::string> tier_sizes =
      result["client-tier-sizes"].as<std::vector<std::string>>();
  if (patterns.size() != tier_sizes.size()) {
    LOG_ERROR("Number of patterns must match number of clients");
    return false;
  }

  for (size_t i = 0; i < patterns.size(); i++) {
    ClientConfig config;
    auto sizes = _split(tier_sizes[i], ' ');

    if (sizes.size() != server_memory_config_.num_tiers) {
      LOG_ERROR("Invalid number of tier sizes for client " << i);
      return false;
    }

    for (const auto &size : sizes) {
      config.tier_sizes.push_back(std::stoul(size));
    }

    if (patterns[i] == "uniform") {
      config.pattern = AccessPattern::UNIFORM;
    } else if (patterns[i] == "hot") {
      config.pattern = AccessPattern::HOT;
    } else if (patterns[i] == "zipfian") {
      config.pattern = AccessPattern::ZIPFIAN;
    } else {
      LOG_ERROR("Invalid pattern type: " << patterns[i]);
      return false;
    }

    client_configs_.push_back(config);
  }
  return true;
}

bool ConfigParser::parse(int argc, char *argv[]) {
  auto result = options_.parse(argc, argv);
  if (result.count("help")) {
    std::cout << options_.help() << std::endl;
    help_requested_ = true;
    return false;
  }

  if (!_parseBasicConfig(result) || !_parseServerConfig(result) ||
      !_parseClientConfigs(result)) {
    return false;
  }

  if (!_validateMemoryConfiguration()) {
    return false;
  }

  _printConfig();
  return true;
}

std::string ConfigParser::_getTierName(size_t tier_idx, size_t num_tiers) {
  if (num_tiers == 2) {
    return (tier_idx == 0) ? "DRAM" : "PMEM";
  }
  return (tier_idx == 0)   ? "Local NUMA"
         : (tier_idx == 1) ? "Remote NUMA"
                           : "PMEM";
}

bool ConfigParser::_validateMemoryConfiguration() {
  std::vector<size_t> tier_totals(server_memory_config_.num_tiers, 0);

  for (const auto &client : client_configs_) {
    for (size_t i = 0; i < server_memory_config_.num_tiers; i++) {
      tier_totals[i] += client.tier_sizes[i];
    }
  }

  const size_t *tier_limits[] = {&server_memory_config_.local_numa.capacity,
                                 &server_memory_config_.remote_numa.capacity,
                                 &server_memory_config_.pmem.capacity};

  for (size_t i = 0; i < server_memory_config_.num_tiers; i++) {
    if (tier_totals[i] > *tier_limits[i]) {
      LOG_ERROR("Memory allocation exceeds "
                << _getTierName(i, server_memory_config_.num_tiers)
                << " limit");
      return false;
    }
  }
  return true;
}

void ConfigParser::_printConfig() const {
  LOG_INFO("========== Configuration Parameters ==========");
  LOG_INFO("Buffer Size: " << buffer_size_);
  LOG_INFO("Running Time: " << running_time_ << " seconds");
  LOG_INFO("Read/Write Ratio: " << rw_ratio_);
  LOG_INFO("Sample Rate: " << sample_rate_);
  LOG_INFO("Migration Page Policy:");
  LOG_INFO("  - Hot Access Interval: " << policy_config_.hot_access_interval
                                       << " ms");
  LOG_INFO("  - Cold Access Interval: " << policy_config_.cold_access_interval
                                        << " ms");
  LOG_INFO("Number of Tiers: " << server_memory_config_.num_tiers);
  LOG_INFO("CDF Output File: " << cdf_output_file_);

  LOG_INFO("Memory Tier Sizes:");
  for (size_t i = 0; i < server_memory_config_.num_tiers; i++) {
    const size_t *sizes[] = {&server_memory_config_.local_numa.capacity,
                             &server_memory_config_.remote_numa.capacity,
                             &server_memory_config_.pmem.capacity};
    LOG_INFO("  - " << _getTierName(i, server_memory_config_.num_tiers) << ": "
                    << *sizes[i] << " pages");
  }

  LOG_INFO("Client Configurations:");
  for (size_t i = 0; i < client_configs_.size(); i++) {
    LOG_INFO("  Client " << i + 1 << ":");
    LOG_INFO("    - Pattern: "
             << (client_configs_[i].pattern == AccessPattern::UNIFORM
                     ? "Uniform"
                     : "Skewed"));
    for (size_t j = 0; j < client_configs_[i].tier_sizes.size(); j++) {
      LOG_INFO("    - " << _getTierName(j, server_memory_config_.num_tiers)
                        << ": " << client_configs_[i].tier_sizes[j]
                        << " pages");
    }
  }
  LOG_INFO("==========================================");
}