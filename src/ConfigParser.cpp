#include "ConfigParser.hpp"
#include <iostream>

ConfigParser::ConfigParser()
    : options_("MemoryTiering", "Concurrent Ring Buffer Demonstration"),
    buffer_size_(10),  // Default values
    message_count_(100),
    help_requested_(false) {

    options_.add_options()
        ("b,buffer-size", "Size of ring buffer",
            cxxopts::value<int>()->default_value("10"))
        ("m,messages", "Number of messages per client",
            cxxopts::value<int>()->default_value("100"))
        ("p,patterns", "Memory access patterns for each client (uniform/skewed)",
            cxxopts::value<std::vector<std::string>>())
        ("s,sizes", "Memory space size for each client",
            cxxopts::value<std::vector<int>>())
        ("c,server", "Memory configuration for server",
            cxxopts::value<std::vector<int>>())
        ("h,help", "Print usage information");
}

bool ConfigParser::parse(int argc, char* argv[]) {
    auto result = options_.parse(argc, argv);

    if (result.count("help")) {
        std::cout << options_.help() << std::endl;
        help_requested_ = true;
        return false;
    }

    buffer_size_ = result["buffer-size"].as<int>();
    message_count_ = result["messages"].as<int>();
    auto patterns = result["patterns"].as<std::vector<std::string>>();
    auto sizes = result["sizes"].as<std::vector<int>>();
    auto server_configs = result["server"].as<std::vector<int>>();

    if (patterns.size() != sizes.size()) {
        std::cerr << "Error: Number of patterns must match number of memory sizes" << std::endl;
        return false;
    }

    for (size_t i = 0; i < patterns.size(); i++) {
        ClientConfig config;
        config.memory_size = sizes[i];

        if (patterns[i] == "uniform") {
            config.pattern = AccessPattern::UNIFORM;
        }
        else if (patterns[i] == "skewed") {
            config.pattern = AccessPattern::SKEWED_70_20_10;
        }
        else {
            std::cerr << "Invalid pattern type: " << patterns[i] << std::endl;
            return false;
        }

        client_configs_.push_back(config);
    }

    // Server configuration, order: local_numa_size, remote_numa_size, pmem_size
    if (server_configs.size() != 1) {
        std::cerr << "Error: Server configuration must have exactly one memory size" << std::endl;
        return false;
    }

    server_memory_config_.local_numa_size = server_configs[0];
    server_memory_config_.remote_numa_size = server_configs[1];
    server_memory_config_.pmem_size = server_configs[2];


    return true;
}