#include "ConfigParser.hpp"
#include "Logger.hpp"

/**
 * Constructor: initializes command line options with default values
 */
ConfigParser::ConfigParser()
    : options_("MemoryTiering", "Concurrent Ring Buffer Demonstration"),
    buffer_size_(10),
    message_count_(100),
    help_requested_(false) {

    options_.add_options()
        ("b,buffer-size", "Size of ring buffer",
            cxxopts::value<size_t>()->default_value("10"))
        ("m,messages", "Number of messages per client",
            cxxopts::value<size_t>()->default_value("100"))
        ("p,patterns", "Memory access patterns for each client (uniform/skewed)",
            cxxopts::value<std::vector<std::string>>())
        ("c,client-addr-space-sizes", "Address space size for each client",
            cxxopts::value<std::vector<size_t>>())
        ("t,num-tiers", "Number of memory tiers",
            cxxopts::value<size_t>()->default_value("3"))
        ("s,mem-sizes", "Memory size for each tiering",
            cxxopts::value<std::vector<size_t>>())
        ("hot-access-cnt", "Access count for determine a hot page",
            cxxopts::value<size_t>()->default_value("10"))
        ("cold-access-interval", "Access interval for determine a cold page",
            cxxopts::value<size_t>()->default_value("1000"))
        ("h,help", "Print usage information");
}

/**
 * Parses command line arguments and validates configuration
 * @param argc Argument count
 * @param argv Argument values
 * @return true if parsing successful, false otherwise
 */
bool ConfigParser::parse(int argc, char* argv[]) {
    auto result = options_.parse(argc, argv);

    // Handle help request
    if (result.count("help")) {
        std::cout << options_.help() << std::endl;
        help_requested_ = true;
        return false;
    }

    // Parse basic parameters
    buffer_size_ = result["buffer-size"].as<size_t>();
    message_count_ = result["messages"].as<size_t>();
    policy_config_.hot_access_cnt = result["hot-access-cnt"].as<size_t>();
    policy_config_.cold_access_interval = result["cold-access-interval"].as<size_t>();

    // Parse client configurations
    auto patterns = result["patterns"].as<std::vector<std::string>>();
    auto client_addr_space_sizes = result["client-addr-space-sizes"].as<std::vector<size_t>>();
    auto mem_sizes = result["mem-sizes"].as<std::vector<size_t>>();

    // Validate client configuration counts match
    if (patterns.size() != client_addr_space_sizes.size()) {
        LOG_ERROR("Error: Number of patterns must match number of memory sizes");
        return false;
    }

    // Process each client's configuration
    for (size_t i = 0; i < patterns.size(); i++) {
        ClientConfig config;
        config.addr_space_size = client_addr_space_sizes[i];

        // Parse access pattern type
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

    // Parse server memory configuration
    size_t num_tiers_ = result["num-tiers"].as<size_t>();
    if (num_tiers_ < 2 || num_tiers_ > 4) {
        LOG_ERROR("Error: Number of tiers must be at least 2 and at most 3");
        return false;
    }
    // Expected order: local_numa_size, remote_numa_size, pmem_size
    if (mem_sizes.size() != 3) {
        LOG_ERROR("Error: Server configuration must have exactly three memory sizes");
        return false;
    }

    server_memory_config_.num_tiers = num_tiers_;
    server_memory_config_.local_numa_size = mem_sizes[0];
    server_memory_config_.remote_numa_size = mem_sizes[1];
    server_memory_config_.pmem_size = mem_sizes[2];

    printConfig();
    return true;
}

/**
 * Print all configuration parameters in a formatted way
 */
void ConfigParser::printConfig() const {
    LOG_INFO("========== Configuration Parameters ==========");

    // Basic parameters
    LOG_INFO("Buffer Size: " << buffer_size_);
    LOG_INFO("Message Count: " << message_count_);

    // Policy configuration
    LOG_INFO("Hot Page Policy:");
    LOG_INFO("  - Hot Access Count: " << policy_config_.hot_access_cnt);
    LOG_INFO("  - Cold Access Interval: " << policy_config_.cold_access_interval << " ms");

    // Number of tiers
    LOG_INFO("Number of Tiers: " << server_memory_config_.num_tiers);
    // Server memory configuration
    LOG_INFO("Memory Tier Sizes:");
   
    if (server_memory_config_.num_tiers == 2) {
        LOG_INFO("  - DRAM: " << server_memory_config_.local_numa_size + server_memory_config_.remote_numa_size  << " pages");
        LOG_INFO("  - PMEM: " << server_memory_config_.pmem_size << " pages");
    }
    if (server_memory_config_.num_tiers == 3 ) {
        LOG_INFO("  - Local NUMA: " << server_memory_config_.local_numa_size << " pages");
        LOG_INFO("  - Remote NUMA: " << server_memory_config_.remote_numa_size << " pages");
        LOG_INFO("  - PMEM: " << server_memory_config_.pmem_size << " pages");
    }

    // Client configurations
    LOG_INFO("Client Configurations:");
    for (size_t i = 0; i < client_configs_.size(); i++) {
        LOG_INFO("  Client " << i + 1 << ":");
        LOG_INFO("    - Pattern: " <<
            (client_configs_[i].pattern == AccessPattern::UNIFORM ? "Uniform" : "Skewed"));
        LOG_INFO("    - Address Space Size: " << client_configs_[i].addr_space_size << " pages");
    }

    LOG_INFO("==========================================");
}