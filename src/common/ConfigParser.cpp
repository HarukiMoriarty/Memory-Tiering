#include "ConfigParser.hpp"
#include "Logger.hpp"

std::vector<std::string> ConfigParser::split(const std::string& s, char delim) {
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
    buffer_size_(10),
    message_count_(100),
    help_requested_(false) {

    options_.add_options()
        ("b,buffer-size", "Size of ring buffer", cxxopts::value<size_t>()->default_value("10"))
        ("m,messages", "Number of messages per client", cxxopts::value<size_t>()->default_value("100"))
        ("p,patterns", "Memory access patterns for each client (uniform/skewed)", cxxopts::value<std::vector<std::string>>())
        ("c,client-tier-sizes", "Memory space size per tier per client (client1_local,client1_remote,client1_pmem ...)", cxxopts::value<std::vector<std::string>>())
        ("t,num-tiers", "Number of memory tiers", cxxopts::value<size_t>()->default_value("3"))
        ("s,mem-sizes", "Memory size in pages for each tier", cxxopts::value<std::vector<size_t>>())
        ("hot-access-cnt", "Hot page threshold count", cxxopts::value<size_t>()->default_value("10"))
        ("cold-access-interval", "Cold page interval (ms)", cxxopts::value<size_t>()->default_value("1000"))
        ("h,help", "Print usage information");
}

bool ConfigParser::parseBasicConfig(const cxxopts::ParseResult& result) {
    buffer_size_ = result["buffer-size"].as<size_t>();
    message_count_ = result["messages"].as<size_t>();
    policy_config_.hot_access_cnt = result["hot-access-cnt"].as<size_t>();
    policy_config_.cold_access_interval = result["cold-access-interval"].as<size_t>();
    server_memory_config_.num_tiers = result["num-tiers"].as<size_t>();

    if (server_memory_config_.num_tiers < 2 || server_memory_config_.num_tiers > 3) {
        LOG_ERROR("Number of tiers must be between 2 and 3");
        return false;
    }
    return true;
}

bool ConfigParser::parseServerConfig(const cxxopts::ParseResult& result) {
    auto mem_sizes = result["mem-sizes"].as<std::vector<size_t>>();
    if (mem_sizes.size() != server_memory_config_.num_tiers) {
        LOG_ERROR("Server configuration must have exactly " << server_memory_config_.num_tiers << " memory sizes");
        return false;
    }

    server_memory_config_.local_numa_capacity = mem_sizes[0];
    server_memory_config_.remote_numa_capacity = (server_memory_config_.num_tiers == 3) ? mem_sizes[1] : 0;
    server_memory_config_.pmem_capacity = mem_sizes.back();
    return true;
}

bool ConfigParser::parseClientConfigs(const std::vector<std::string>& patterns,
    const std::vector<std::string>& tier_sizes) {
    if (patterns.size() != tier_sizes.size()) {
        LOG_ERROR("Number of patterns must match number of clients");
        return false;
    }

    for (size_t i = 0; i < patterns.size(); i++) {
        ClientConfig config;
        auto sizes = split(tier_sizes[i], ' ');

        if (sizes.size() != server_memory_config_.num_tiers) {
            LOG_ERROR("Invalid number of tier sizes for client " << i);
            return false;
        }

        for (const auto& size : sizes) {
            config.tier_sizes.push_back(std::stoul(size));
        }

        if (patterns[i] == "uniform") {
            config.pattern = AccessPattern::UNIFORM;
        }
        else if (patterns[i] == "skewed") {
            config.pattern = AccessPattern::SKEWED_70_20_10;
        }
        else {
            LOG_ERROR("Invalid pattern type: " << patterns[i]);
            return false;
        }

        client_configs_.push_back(config);
    }
    return true;
}

bool ConfigParser::parse(int argc, char* argv[]) {
    auto result = options_.parse(argc, argv);
    if (result.count("help")) {
        std::cout << options_.help() << std::endl;
        help_requested_ = true;
        return false;
    }

    if (!parseBasicConfig(result) || !parseServerConfig(result)) {
        return false;
    }

    if (!parseClientConfigs(result["patterns"].as<std::vector<std::string>>(),
        result["client-tier-sizes"].as<std::vector<std::string>>())) {
        return false;
    }

    if (!validateMemoryConfiguration()) {
        return false;
    }

    printConfig();
    return true;
}

std::string ConfigParser::getTierName(size_t tier_idx, size_t num_tiers) {
    if (num_tiers == 2) {
        return (tier_idx == 0) ? "DRAM" : "PMEM";
    }
    return (tier_idx == 0) ? "Local NUMA" :
        (tier_idx == 1) ? "Remote NUMA" : "PMEM";
}

bool ConfigParser::validateMemoryConfiguration() {
    std::vector<size_t> tier_totals(server_memory_config_.num_tiers, 0);

    for (const auto& client : client_configs_) {
        for (size_t i = 0; i < server_memory_config_.num_tiers; i++) {
            tier_totals[i] += client.tier_sizes[i];
        }
    }

    const size_t* tier_limits[] = {
        &server_memory_config_.local_numa_capacity,
        &server_memory_config_.remote_numa_capacity,
        &server_memory_config_.pmem_capacity
    };

    for (size_t i = 0; i < server_memory_config_.num_tiers; i++) {
        if (tier_totals[i] > *tier_limits[i]) {
            LOG_ERROR("Memory allocation exceeds " << getTierName(i, server_memory_config_.num_tiers) << " limit");
            return false;
        }
    }
    return true;
}

void ConfigParser::printConfig() const {
    LOG_INFO("========== Configuration Parameters ==========");
    LOG_INFO("Buffer Size: " << buffer_size_);
    LOG_INFO("Message Count: " << message_count_);
    LOG_INFO("Hot Page Policy:");
    LOG_INFO("  - Hot Access Count: " << policy_config_.hot_access_cnt);
    LOG_INFO("  - Cold Access Interval: " << policy_config_.cold_access_interval << " ms");
    LOG_INFO("Number of Tiers: " << server_memory_config_.num_tiers);

    LOG_INFO("Memory Tier Sizes:");
    for (size_t i = 0; i < server_memory_config_.num_tiers; i++) {
        const size_t* sizes[] = {
            &server_memory_config_.local_numa_capacity,
            &server_memory_config_.remote_numa_capacity,
            &server_memory_config_.pmem_capacity
        };
        LOG_INFO("  - " << getTierName(i, server_memory_config_.num_tiers) << ": " << *sizes[i] << " pages");
    }

    LOG_INFO("Client Configurations:");
    for (size_t i = 0; i < client_configs_.size(); i++) {
        LOG_INFO("  Client " << i + 1 << ":");
        LOG_INFO("    - Pattern: " <<
            (client_configs_[i].pattern == AccessPattern::UNIFORM ? "Uniform" : "Skewed"));
        for (size_t j = 0; j < client_configs_[i].tier_sizes.size(); j++) {
            LOG_INFO("    - " << getTierName(j, server_memory_config_.num_tiers) <<
                ": " << client_configs_[i].tier_sizes[j] << " pages");
        }
    }
    LOG_INFO("==========================================");
}